#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <lwip/sockets.h>

#include <atomic>

#include "local_upload.h"
#include "status_led.h"
#include "stm32_image.h"
#include "stm32_programmer.h"

static const IPAddress ap_ip(192, 168, 4, 1);
static constexpr char ap_host[] = "192.168.4.1";
static constexpr char partial_path[] = "/stm32.local.part";
static bool filesystem_ready;
// Dang nhan file upload: chan nap STM32 cho toi khi upload xong
static std::atomic<bool> upload_busy{false};

static constexpr char page[] = R"HTML(<!doctype html>
<html lang="vi">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Upload firmware STM32</title>
<style>
body{font:1rem system-ui;color:#172336;background:#f3f5f8;margin:0;padding:1rem}
main{max-width:32rem;margin:8vh auto;background:white;padding:clamp(1rem,5vw,2rem);border:1px solid #d7dee8;border-radius:1rem}
h1{font-size:1.6rem;margin-top:0}label,input,progress,button{display:block;margin:1rem 0}
button{font:inherit;background:#17478e;color:white;padding:.7rem 1rem;border:0;border-radius:.5rem}
button:disabled{opacity:.6}progress{width:100%}input{max-width:100%}
</style>
<main>
<h1>Upload firmware STM32</h1>
<p>Chọn file đã build cho STM32F103C8T6. Tối đa 64 KiB.</p>
<form id="upload">
<label for="firmware">File firmware (.bin)</label>
<input id="firmware" type="file" accept=".bin" required>
<button id="submit">Upload firmware</button>
</form>
<progress id="progress" max="100" value="0" aria-label="Tiến độ tải file"></progress>
<p id="status" role="status">Chưa chọn file.</p>
<hr>
<h2>Nạp STM32</h2>
<p>Chỉ nạp khi loa đã phát xong. ESP32 tự điều khiển BOOT0 và NRST.</p>
<fieldset>
<legend>Nguồn file</legend>
<label><input type="radio" name="source" value="local" checked> File upload từ trình duyệt</label>
<label><input type="radio" name="source" value="ota"> File tải qua HTTPS (OTA)</label>
</fieldset>
<button id="program" type="button">Nạp STM32</button>
<p id="program-status" role="status">Chưa nạp.</p>
</main>
<script>
const form=document.querySelector('#upload'), input=document.querySelector('#firmware');
const button=document.querySelector('#submit'), progress=document.querySelector('#progress');
const status=document.querySelector('#status');
const program=document.querySelector('#program'), programStatus=document.querySelector('#program-status');
function busy(value) { input.disabled=button.disabled=program.disabled=value; }
async function pollProgram(response) {
  while (response.status===202) {
    programStatus.textContent=await response.text();
    await new Promise(resolve=>setTimeout(resolve, 800));
    response=await fetch('/program', {cache:'no-store'});
  }
  programStatus.textContent=await response.text();
}
program.addEventListener('click', async () => {
  if (!confirm('Loa đã phát xong? Nạp sẽ thay firmware đang chạy.')) return;
  const source=document.querySelector('input[name=source]:checked').value;
  busy(true);
  try {
    const response=await fetch('/program', {method:'POST', headers:{'X-STM32-Upload':'1','X-STM32-Source':source}});
    await pollProgram(response);
  } catch {
    programStatus.textContent='Mất kết nối với ESP32. Mở lại trang để xem trạng thái nạp.';
  } finally {
    busy(false);
  }
});
(async () => {
  busy(true);
  try {
    const response=await fetch('/program', {cache:'no-store'});
    if (response.status===202) busy(true);
    await pollProgram(response);
  } catch {
    programStatus.textContent='Chưa đọc được trạng thái nạp.';
  } finally {
    busy(false);
  }
})();
form.addEventListener('submit', e => {
  e.preventDefault();
  const file=input.files[0];
  progress.value=0;
  if (!file || !file.name.toLowerCase().endsWith('.bin') || file.size<8 || file.size>65536) {
    status.textContent='Chọn file .bin có kích thước hợp lệ.';
    return;
  }
  busy(true);
  status.textContent='Đang gửi firmware…';
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/upload');
  xhr.timeout=40000;
  xhr.setRequestHeader('Content-Type','application/octet-stream');
  xhr.setRequestHeader('X-STM32-Upload','1');
  xhr.upload.onprogress=e=>{
    if (e.lengthComputable) progress.value=100*e.loaded/e.total;
  };
  xhr.onload=()=>{
    if (xhr.status===200) progress.value=100;
    status.textContent=xhr.responseText || 'ESP32 không nhận được file.';
  };
  xhr.onerror=()=>{status.textContent='Kết nối bị ngắt trước khi ESP32 trả kết quả.';};
  xhr.ontimeout=()=>{status.textContent='ESP32 không trả kết quả trong 40 giây.';};
  xhr.onloadend=()=>busy(false);
  xhr.send(file);
});
</script>
</html>)HTML";

static bool on_ap(httpd_req_t *req)
{
    sockaddr_storage address = {};
    socklen_t length = sizeof(address);
    char host[sizeof(ap_host) + sizeof(":80") - 1];

    if ((getsockname(httpd_req_to_sockfd(req), reinterpret_cast<sockaddr *>(&address), &length) != 0) ||
        (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) ||
        (strncmp(host, ap_host, sizeof(ap_host) - 1) != 0))
    {
        return false;
    }

    const char *port = host + sizeof(ap_host) - 1;

    if ((*port != '\0') && (strcmp(port, ":80") != 0))
    {
        return false;
    }

    if ((address.ss_family == AF_INET) && (length >= sizeof(sockaddr_in)))
    {
        const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(&address);
        return ipv4->sin_addr.s_addr == uint32_t(ap_ip);
    }

#if LWIP_IPV6
    // ESP-IDF's dual-stack listener reports IPv4 connections as mapped IPv6.
    if ((address.ss_family == AF_INET6) && (length >= sizeof(sockaddr_in6)))
    {
        constexpr uint8_t mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        const auto *ipv6 = reinterpret_cast<const sockaddr_in6 *>(&address);
        const uint8_t *bytes = ipv6->sin6_addr.s6_addr;
        return (memcmp(bytes, mapped_prefix, sizeof(mapped_prefix)) == 0) &&
               (ap_ip == bytes + sizeof(mapped_prefix));
    }
#endif

    return false;
}

static esp_err_t reply(httpd_req_t *req, const char *status, const char *message, bool close = false)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    const esp_err_t result = httpd_resp_sendstr(req, message);
    // Close rejected requests before the server tries to drain the remaining body.
    return close ? ESP_FAIL : result;
}

static esp_err_t show_page(httpd_req_t *req)
{
    if (!on_ap(req))
    {
        return reply(req, "403 Forbidden", "Hãy kết nối Wi-Fi của ESP32.");
    }

    if (!filesystem_ready)
    {
        return reply(req, "503 Service Unavailable",
                     "Chưa mở được LittleFS. Kiểm tra Serial Monitor và khởi tạo LittleFS nếu phân vùng còn trống.");
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, sizeof(page) - 1);
}

enum class ReceiveResult
{
    Complete,
    Interrupted,
    StorageError
};

static ReceiveResult receive_file(httpd_req_t *req, File &file)
{
    char buffer[1024];
    const unsigned long start = millis();
    bool storage_ok = static_cast<bool>(file);

    for (size_t left = req->content_len; left > 0;)
    {
        if ((millis() - start) >= 30000)
        {
            return ReceiveResult::Interrupted;
        }

        const size_t count = (left < sizeof(buffer)) ? left : sizeof(buffer);
        const int got = httpd_req_recv(req, buffer, count);

        if (got <= 0)
        {
            return ReceiveResult::Interrupted;
        }

        // Drain the bounded body even after a write error so the browser receives the HTTP error.
        if (storage_ok)
        {
            storage_ok = file.write(reinterpret_cast<const uint8_t *>(buffer), got) ==
                         static_cast<size_t>(got);
        }

        left -= static_cast<size_t>(got);
    }

    return storage_ok ? ReceiveResult::Complete : ReceiveResult::StorageError;
}

static bool from_upload_page(httpd_req_t *req)
{
    char marker[2];
    return on_ap(req) &&
           (httpd_req_get_hdr_value_str(req, "X-STM32-Upload", marker, sizeof(marker)) == ESP_OK) &&
           (strcmp(marker, "1") == 0);
}

static esp_err_t upload(httpd_req_t *req)
{
    char type[32];

    if (!from_upload_page(req) ||
        (httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type)) != ESP_OK) ||
        (strcmp(type, "application/octet-stream") != 0))
    {
        return reply(req, "403 Forbidden", "Hãy upload từ trang của ESP32.", true);
    }

    if ((req->content_len < STM32_VECTOR_BYTES) || (req->content_len > STM32_IMAGE_MAX_BYTES))
    {
        return reply(req, "400 Bad Request", "Kích thước file không hợp lệ.", true);
    }

    bool expected = false;

    if (!upload_busy.compare_exchange_strong(expected, true))
    {
        return reply(req, "409 Conflict", "Đang có một lần upload khác.", true);
    }

    // Xoa co upload_busy tren moi duong ra cua ham
    struct UploadGuard
    {
        ~UploadGuard() { upload_busy.store(false); }
    } guard;

    if (stm32_program_busy())
    {
        return reply(req, "409 Conflict", "STM32 đang nạp. Hãy thử upload lại sau.", true);
    }

    File file = filesystem_ready ? LittleFS.open(partial_path, FILE_WRITE) : File();

    status_led_set_upload_active(true);
    const ReceiveResult received = receive_file(req, file);
    status_led_set_upload_active(false);
    file.close();

    if (received != ReceiveResult::Complete)
    {
        if (filesystem_ready)
        {
            LittleFS.remove(partial_path);
        }

        if (received == ReceiveResult::Interrupted)
        {
            Serial.println("Local upload: request body interrupted");
            return reply(req, "400 Bad Request", "Nhận file bị gián đoạn.", true);
        }

        if (!filesystem_ready)
        {
            return reply(req, "503 Service Unavailable", "LittleFS chưa sẵn sàng.");
        }

        Serial.println("Local upload: could not write the temporary file");
        return reply(req, "500 Internal Server Error", "Không ghi được file tạm vào LittleFS.");
    }

    file = LittleFS.open(partial_path, FILE_READ);
    uint8_t vectors[STM32_VECTOR_BYTES];
    const bool valid = file && (file.size() == req->content_len) &&
                       (file.read(vectors, sizeof(vectors)) == sizeof(vectors)) &&
                       stm32_image_valid(vectors, req->content_len);
    file.close();

    if (!valid)
    {
        LittleFS.remove(partial_path);
        return reply(req, "400 Bad Request", "File không có vector khởi động hợp lệ cho STM32.");
    }

    if (!LittleFS.rename(partial_path, STM32_LOCAL_IMAGE_PATH))
    {
        LittleFS.remove(partial_path);
        return reply(req, "500 Internal Server Error", "Không lưu được firmware.");
    }

    return reply(req, "200 OK", "Đã lưu firmware. Sẵn sàng nạp STM32.");
}

static esp_err_t program_status(httpd_req_t *req)
{
    if (!on_ap(req))
    {
        return reply(req, "403 Forbidden", "Hãy kết nối Wi-Fi của ESP32.");
    }

    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const bool busy = stm32_program_busy();
    return reply(req, busy ? "202 Accepted" : "200 OK", stm32_program_status());
}

static esp_err_t request_program(httpd_req_t *req)
{
    if (!from_upload_page(req) || req->content_len != 0)
    {
        return reply(req, "403 Forbidden", "Hãy nạp từ trang của ESP32.", true);
    }

    char source[6] = "";
    (void)httpd_req_get_hdr_value_str(req, "X-STM32-Source", source, sizeof(source));
    const bool from_ota = (strcmp(source, "ota") == 0);

    if (!from_ota && (strcmp(source, "local") != 0))
    {
        return reply(req, "400 Bad Request", "Chọn nguồn file: upload hoặc OTA.");
    }

    if (upload_busy.load())
    {
        return reply(req, "409 Conflict", "Đang upload file. Chờ upload xong rồi nạp.");
    }

    if (!filesystem_ready ||
        !LittleFS.exists(from_ota ? STM32_OTA_IMAGE_PATH : STM32_LOCAL_IMAGE_PATH))
    {
        return reply(req, "409 Conflict", from_ota ? "Chưa có file OTA đã kiểm tra." :
                                                     "Hãy upload file STM32 trước.");
    }

    switch (stm32_program_request(from_ota))
    {
        case Stm32ProgramRequest::Accepted:
            break;

        case Stm32ProgramRequest::AudioPlaying:
            return reply(req, "409 Conflict", "Loa đang phát. Chờ phát xong rồi nạp.");

        case Stm32ProgramRequest::Busy:
        default:
            return reply(req, "409 Conflict", "STM32 đang nạp.");
    }

    return reply(req, "202 Accepted", "Đã yêu cầu nạp STM32…");
}

void local_upload_init(bool storage_ready)
{
    filesystem_ready = storage_ready;

    if (!filesystem_ready)
    {
        Serial.println("Local upload: LittleFS mount failed");
    }

    WiFi.persistent(false);

    // AP mo, khong mat khau: may nao noi vao duoc deu upload va nap duoc STM32
    if (!WiFi.softAP("STM32-OTA") ||
        !WiFi.softAPConfig(ap_ip, ap_ip, IPAddress(255, 255, 255, 0)))
    {
        WiFi.softAPdisconnect(true);
        Serial.println("Local upload: access point did not start");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144;
    httpd_handle_t server = nullptr;
    httpd_uri_t root = {};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = show_page;
    httpd_uri_t post = {};
    post.uri = "/upload";
    post.method = HTTP_POST;
    post.handler = upload;
    httpd_uri_t program_get = {};
    program_get.uri = "/program";
    program_get.method = HTTP_GET;
    program_get.handler = program_status;
    httpd_uri_t program_post = program_get;
    program_post.method = HTTP_POST;
    program_post.handler = request_program;

    if ((httpd_start(&server, &config) != ESP_OK) ||
        (httpd_register_uri_handler(server, &root) != ESP_OK) ||
        (httpd_register_uri_handler(server, &post) != ESP_OK) ||
        (httpd_register_uri_handler(server, &program_get) != ESP_OK) ||
        (httpd_register_uri_handler(server, &program_post) != ESP_OK))
    {
        if (server != nullptr)
        {
            httpd_stop(server);
        }

        WiFi.softAPdisconnect(true);
        Serial.println("Local upload: web server did not start");
        return;
    }

    Serial.printf("Local upload: Wi-Fi STM32-OTA (open), http://%s/\n", ap_host);
}
