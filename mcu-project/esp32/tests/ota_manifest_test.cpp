#include "ota_manifest.h"

#include <cassert>
#include <cstdio>

static bool parse(const char *target, const char *url, const char *size, const char *sha256,
                  OtaManifest &manifest)
{
    char body[512];
    const int length = std::snprintf(body, sizeof(body),
        "{\"target\":\"%s\",\"version\":\"1.2.0\",\"url\":\"%s\",\"size\":%s,\"sha256\":\"%s\"}",
        target, url, size, sha256);
    assert(length > 0 && static_cast<size_t>(length) < sizeof(body));
    return ota_parse_manifest(body, static_cast<size_t>(length) + 1, manifest);
}

int main()
{
    const char *target = "STM32F103C8T6";
    const char *url = "https://example.org/stm32.bin";
    const char *hash = "BA7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    OtaManifest manifest = {};

    assert(parse(target, url, "65536", hash, manifest));
    assert(manifest.size == 65536);
    assert(manifest.sha256[0] == 0xba && manifest.sha256[31] == 0xad);
    assert(!parse("OTHER", url, "65536", hash, manifest));
    assert(!parse(target, "http://example.org/stm32.bin", "8", hash, manifest));
    assert(!parse(target, url, "65537", hash, manifest));
    assert(!parse(target, url, "8.5", hash, manifest));
    assert(!parse(target, url, "8", "bad-hash", manifest));
    const char wrong_root[] = "[{\"target\":\"STM32F103C8T6\"}]";
    assert(!ota_parse_manifest(wrong_root, sizeof(wrong_root), manifest));
}
