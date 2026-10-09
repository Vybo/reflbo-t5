#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zlib.h>

/* Golden files for the host tests and renderers: a path ending ".gz" is gzip-compressed (the T5's PGMs, 518 KB
 * each unpacked), any other plain (the RLCD's PBMs). Link ZLIB::ZLIB. */

static inline bool golden_is_gz(const char *path)
{
    size_t n = strlen(path);
    return n > 3 && strcmp(path + n - 3, ".gz") == 0;
}

/* Writes n bytes to path; false on any error. */
static inline bool golden_write(const char *path, const uint8_t *data, size_t n)
{
    if (golden_is_gz(path)) {
        gzFile f = gzopen(path, "wb9");
        if (f == NULL) {
            return false;
        }
        bool ok = gzwrite(f, data, (unsigned)n) == (int)n;
        return gzclose(f) == Z_OK && ok;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    bool ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* Reads up to size bytes of path into buf: the count read, 0 if it can't be opened or read. */
static inline size_t golden_read(const char *path, uint8_t *buf, size_t size)
{
    if (golden_is_gz(path)) {
        gzFile f = gzopen(path, "rb");
        if (f == NULL) {
            return 0;
        }
        int n = gzread(f, buf, (unsigned)size);
        gzclose(f);
        return n > 0 ? (size_t)n : 0;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    size_t n = fread(buf, 1, size, f);
    fclose(f);
    return n;
}
