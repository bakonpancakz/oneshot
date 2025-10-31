#include <stdio.h>
#pragma once

static inline unsigned long long read_u64_le(FILE* h) {
    unsigned char b[8];
    fread(&b, sizeof(unsigned char), sizeof(b), h);
    unsigned long long v =
        ((unsigned long long)b[7] << 56) |
        ((unsigned long long)b[6] << 48) |
        ((unsigned long long)b[5] << 40) |
        ((unsigned long long)b[4] << 32) |
        ((unsigned long long)b[3] << 24) |
        ((unsigned long long)b[2] << 16) |
        ((unsigned long long)b[1] << 8) |
        ((unsigned long long)b[0]);
    return v;
}

static inline unsigned int read_u32_le(FILE* h) {
    unsigned char b[4];
    fread(&b, sizeof(unsigned char), sizeof(b), h);
    unsigned int v =
        ((unsigned int)b[3] << 24) |
        ((unsigned int)b[2] << 16) |
        ((unsigned int)b[1] << 8) |
        ((unsigned int)b[0]);
    return v;
}

static inline unsigned short read_u16_le(FILE* h) {
    unsigned char b[2];
    fread(&b, sizeof(unsigned char), sizeof(b), h);
    unsigned short v =
        ((unsigned short)b[1] << 8) |
        ((unsigned short)b[0]);
    return v;
}

static inline unsigned char read_u8(FILE* h) {
    unsigned char b;
    fread(&b, sizeof(unsigned char), sizeof(b), h);
    return b;
}