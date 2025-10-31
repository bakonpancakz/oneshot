#include <stdlib.h>
#include <string.h>
#pragma once
#pragma pack(push, 1)

typedef struct {
    unsigned short fileType;        // The file type; must be 0x4d42 (the ASCII string "BM").
    unsigned int fileSize;          // The size, in bytes, of the bitmap bithead.
    unsigned short reserved1;       // Reserved; must be zero.
    unsigned short reserved2;       // Reserved; must be zero.
    unsigned int byteOffset;        // The offset, in bytes, from the beginning of the BITMAPFILEHEADER structure to the bitmap bits.
} BitmapFileHeader;

typedef struct {
    unsigned int structSize;        // The number of bytes required by the structure.
    int width;                      // The width of the bitmap, in pixels.
    int height;                     // The height of the bitmap, in pixels.
    unsigned short planes;          // The number of planes for the target device. This value must be set to 1.
    unsigned short bitCount;        // The number of bits-per-pixel.
    unsigned int compression;       // The type of compression for a compressed bottom-up bitmap.
    unsigned int size;              // The size, in bytes, of the image.
    int xPixelsPerMeter;            // The horizontal resolution, in pixels-per-meter, of the target device for the bitmap.
    int yPixelsPerMeter;            // The vertical resolution, in pixels-per-meter, of the target device for the bitmap.
    unsigned int colorsUsed;        // The number of color indexes in the color table that are actually used by the bitmap.
    unsigned int colorsImportant;   // The number of color indexes that are required for displaying the bitmap.
} BitmapInfoHeader;

#pragma pack(pop)

static const unsigned short MAGIC_BITMAP = ('B' | 'M' << 8);

typedef enum {
    BMP_OK = 0,
    BMP_MEMORY_ERROR = 1,
    BMP_INVALID_ARGUMENTS = 2,
    BMP_UNEXPECTED_EOF = 3,
    BMP_NOT_A_BITMAP_IMAGE = 101,
    BMP_INVALID_HEADER = 102,
    BMP_UNSUPPORTED_BITCOUNT = 103,
    BMP_INVALID_DIMENSIONS = 104,
} bmp_error_t;

static inline bmp_error_t bmp_decode(
    const unsigned char* input_buffer,  // Encoded Buffer
    const unsigned int input_length,    // Encoded Buffer Length
    unsigned int** image_rgba,          // Image Pixels (RGBA)
    unsigned int* image_width,          // Array Width
    unsigned int* image_height          // Array Height
) {
    if (!input_buffer || !image_rgba || !image_width || !image_height) {
        return BMP_INVALID_ARGUMENTS;
    }

    // Read File Header
    // Note: This section here is not cross-platform, replace these with
    //       little-endianreaders if you need it to be.
    BitmapFileHeader bithead;
    BitmapInfoHeader bitinfo;
    if (input_length < sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader)) {
        return BMP_UNEXPECTED_EOF;
    }
    memcpy(&bithead, input_buffer, sizeof(BitmapFileHeader));
    memcpy(&bitinfo, input_buffer + sizeof(BitmapFileHeader), sizeof(BitmapInfoHeader));

    if (bithead.fileType != MAGIC_BITMAP) {
        return BMP_NOT_A_BITMAP_IMAGE;
    }
    if (bithead.reserved1 != 0 || bithead.reserved2 != 0) {
        return BMP_INVALID_HEADER;
    }
    if (bitinfo.bitCount != 24 && bitinfo.bitCount != 32) {
        return BMP_UNSUPPORTED_BITCOUNT;
    }
    if (bitinfo.height == 0 || bitinfo.width == 0) {
        return BMP_INVALID_DIMENSIONS;
    }

    // Copy Pixels
    unsigned int output_offset = 0;
    unsigned int* output_buffer = malloc(bitinfo.height * bitinfo.width * sizeof(unsigned int));
    if (!output_buffer) {
        return BMP_MEMORY_ERROR;
    }

    unsigned int alpha = (bitinfo.bitCount == 32);
    unsigned int stride = (bitinfo.width * (alpha ? 4 : 3) + 3) & ~3;
    unsigned int length = bithead.byteOffset + (stride * bitinfo.height);
    unsigned int row_start = bithead.byteOffset + (bitinfo.height - 1) * stride;
    if (length > input_length) {
        return BMP_UNEXPECTED_EOF;
    }
    for (int y = 0; y < bitinfo.height; y++) {
        unsigned int input_offset = row_start;
        for (int x = 0; x < bitinfo.width; x++) {
            unsigned char b = input_buffer[input_offset++];
            unsigned char g = input_buffer[input_offset++];
            unsigned char r = input_buffer[input_offset++];
            unsigned char a = 0xFF;
            if (alpha) {
                a = input_buffer[input_offset++];
            }
            output_buffer[output_offset++] = (r << 24) | (g << 16) | (b << 8) | a;
        }
        row_start -= stride;
    }

    *image_height = bitinfo.height;
    *image_width = bitinfo.width;
    *image_rgba = output_buffer;
    return BMP_OK;
}