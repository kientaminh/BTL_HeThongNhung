#pragma once

#include <stddef.h>
#include <stdint.h>

struct OtaManifest
{
    char version[32];
    char url[384];
    size_t size;
    uint8_t sha256[32];
};

bool ota_parse_manifest(const char *body, size_t length_with_null, OtaManifest &manifest);
