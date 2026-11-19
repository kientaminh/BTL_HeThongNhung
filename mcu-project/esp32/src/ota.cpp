#include <Arduino.h>

#include "ota.h"

#if __has_include("ota_local.h")

#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "ota_local.h"
#include "ota_manifest.h"
#include "status_led.h"
#include "stm32_programmer.h"

static constexpr char partial_path[] = "/stm32.part";
static constexpr const char *ready_path = STM32_OTA_IMAGE_PATH;
// Thu ket noi lai router toi da moi khoang nay khi chua co ai noi vao AP
static constexpr unsigned long sta_retry_ms = 30000;
static constexpr int max_manifest_bytes = 1024;

static bool start_get(HTTPClient &http, WiFiClientSecure &tls, const char *url)
{
    if (strncmp(url, "https://", 8) != 0)
    {
        return false;
    }

    tls.setCACert(OTA_ROOT_CA);
    tls.setHandshakeTimeout(10);
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

    if (!http.begin(tls, url))
    {
        return false;
    }

    const char *headers[] = {"Transfer-Encoding", "Content-Encoding"};
    http.collectHeaders(headers, 2);

    if (http.GET() != HTTP_CODE_OK)
    {
        return false;
    }

    const String transfer = http.header(headers[0]);
    const String encoding = http.header(headers[1]);
    return (transfer.isEmpty() || transfer.equalsIgnoreCase("identity")) &&
           (encoding.isEmpty() || encoding.equalsIgnoreCase("identity"));
}

static bool fetch_manifest(OtaManifest &manifest)
{
    WiFiClientSecure tls;
    HTTPClient http;

    if (!start_get(http, tls, OTA_MANIFEST_URL))
    {
        return false;
    }

    const int size = http.getSize();

    if ((size <= 0) || (size > max_manifest_bytes))
    {
        return false;
    }

    const String body = http.getString();
    return (body.length() == static_cast<unsigned>(size)) &&
           ota_parse_manifest(body.c_str(), body.length() + 1, manifest);
}

static bool file_matches(const char *path, const OtaManifest &manifest)
{
    File file = LittleFS.open(path, FILE_READ);

    if (!file || (file.size() != manifest.size))
    {
        return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    bool ok = mbedtls_sha256_starts_ret(&sha, 0) == 0;
    uint8_t buffer[1024];
    uint8_t digest[32];

    for (size_t left = manifest.size; ok && left > 0;)
    {
        const size_t count = (left < sizeof(buffer)) ? left : sizeof(buffer);
        ok = (file.read(buffer, count) == count) &&
             (mbedtls_sha256_update_ret(&sha, buffer, count) == 0);
        left -= count;
    }

    ok = ok && (mbedtls_sha256_finish_ret(&sha, digest) == 0);
    mbedtls_sha256_free(&sha);
    return ok && (memcmp(digest, manifest.sha256, sizeof(digest)) == 0);
}

static bool download(const OtaManifest &manifest)
{
    File file = LittleFS.open(partial_path, FILE_WRITE);

    if (!file)
    {
        return false;
    }

    WiFiClientSecure tls;
    HTTPClient http;
    const int expected = static_cast<int>(manifest.size);

    status_led_set_download_active(true);
    const bool received = start_get(http, tls, manifest.url) &&
                          (http.getSize() == expected) &&
                          (http.writeToStream(&file) == expected);
    http.end();
    status_led_set_download_active(false);
    file.close();

    if (received && file_matches(partial_path, manifest) && LittleFS.rename(partial_path, ready_path))
    {
        return true;
    }

    LittleFS.remove(partial_path);
    return false;
}

// ESP32 chi co 1 kenh RF: STA quet/ket noi router se keo AP STM32-OTA doi kenh, lam
// dien thoai dang o trang nap rot ket noi. Nen khi co may noi vao AP thi ngat STA, khong
// tu ket noi lai; chi ket noi router khi AP khong co ai
static bool station_ready(bool &started, unsigned long &last_begin)
{
    if (WiFi.softAPgetStationNum() > 0)
    {
        if (started)
        {
            WiFi.disconnect(false);
            started = false;
        }

        return false;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    if (!started || ((millis() - last_begin) >= sta_retry_ms))
    {
        WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
        started = true;
        last_begin = millis();
    }

    return false;
}

static void ota_task(void *)
{
    bool started = false;
    unsigned long last_begin = 0;

    // Wait for the station and SNTP before verifying certificate validity dates.
    configTime(0, 0, "pool.ntp.org");

    for (;;)
    {
        if (!station_ready(started, last_begin) || (time(nullptr) < 1704067200))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        OtaManifest manifest = {};

        if (!fetch_manifest(manifest))
        {
            Serial.println("OTA: manifest rejected or unavailable");
        }
        else if (!file_matches(ready_path, manifest))
        {
            if (download(manifest))
            {
                Serial.printf("OTA: STM32 %s verified and saved to %s\n", manifest.version, ready_path);
            }
            else
            {
                Serial.println("OTA: download or SHA-256 verification failed");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void ota_init(bool storage_ready)
{
    if (!storage_ready)
    {
        Serial.println("OTA: LittleFS mount failed; provision LittleFS first");
        return;
    }

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    // ota_task tu ket noi lai, va chi khi AP khong co ai (xem station_ready)
    WiFi.setAutoReconnect(false);

    if (xTaskCreate(ota_task, "stm32_ota", 12288, nullptr, 1, nullptr) != pdPASS)
    {
        Serial.println("OTA: download task did not start");
    }
}

#else

void ota_init(bool)
{
    Serial.println("OTA: configure include/ota_local.h to enable downloads");
}

#endif
