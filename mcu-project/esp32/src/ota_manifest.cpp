#include "ota_manifest.h"

#include <cJSON.h>
#include <string.h>

static int hex_digit(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }

    if ((c >= 'a') && (c <= 'f'))
    {
        return c - 'a' + 10;
    }

    if ((c >= 'A') && (c <= 'F'))
    {
        return c - 'A' + 10;
    }

    return -1;
}

static bool read_manifest(const cJSON *json, OtaManifest &manifest)
{
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(json, "target");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(json, "url");
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(json, "size");
    const cJSON *sha256 = cJSON_GetObjectItemCaseSensitive(json, "sha256");

    if (!cJSON_IsObject(json) ||
        !cJSON_IsString(target) || (strcmp(target->valuestring, "STM32F103C8T6") != 0) ||
        !cJSON_IsString(version) || (version->valuestring[0] == '\0') ||
        (strlen(version->valuestring) >= sizeof(manifest.version)) ||
        !cJSON_IsString(url) || (strncmp(url->valuestring, "https://", 8) != 0) ||
        (url->valuestring[8] == '\0') || (strlen(url->valuestring) >= sizeof(manifest.url)) ||
        !cJSON_IsNumber(size) || !(size->valuedouble >= 8 && size->valuedouble <= 65536) ||
        (size->valuedouble != size->valueint) ||
        !cJSON_IsString(sha256) || (strlen(sha256->valuestring) != 64))
    {
        return false;
    }

    for (size_t i = 0; i < sizeof(manifest.sha256); ++i)
    {
        const int high = hex_digit(sha256->valuestring[2 * i]);
        const int low = hex_digit(sha256->valuestring[2 * i + 1]);

        if ((high < 0) || (low < 0))
        {
            return false;
        }

        manifest.sha256[i] = static_cast<uint8_t>((high << 4) | low);
    }

    strcpy(manifest.version, version->valuestring);
    strcpy(manifest.url, url->valuestring);
    manifest.size = static_cast<size_t>(size->valueint);
    return true;
}

bool ota_parse_manifest(const char *body, size_t length_with_null, OtaManifest &manifest)
{
    cJSON *json = cJSON_ParseWithLengthOpts(body, length_with_null, nullptr, true);
    const bool valid = read_manifest(json, manifest);
    cJSON_Delete(json);
    return valid;
}
