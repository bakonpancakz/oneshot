#include <stdlib.h>
#include <string.h>
#pragma once

static const unsigned int MAGIC_QOIF = ('q' << 24) | ('o' << 16) | ('i' << 8) | ('f');

typedef enum {
    QOI_OK = 0,
    QOI_MEMORY_ERROR = 1,
    QOI_INVALID_ARGUMENTS = 2,
    QOI_UNEXPECTED_EOF = 3,
    QOI_NOT_A_QOI_FILE = 101,
    QOI_INVALID_HEADER = 102,
    QOI_INVALID_COLORSPACE = 103
} qoi_error_t;

#define QOI_MAX_DIMENSION    16384
#define QOI_HEADER_SIZE      14
#define QOI_FOOTER_SIZE      8
#define QOI_MINIMUM_SIZE     QOI_HEADER_SIZE + QOI_FOOTER_SIZE
#define QOI_OP_MASK          0xC0
#define QOI_OP_RGB           0xFE
#define QOI_OP_RGBA          0xFF
#define QOI_OP_RUN           0xC0
#define QOI_OP_INDEX         0x00
#define QOI_OP_DIFF          0x40
#define QOI_OP_LUMA          0x80

static inline unsigned int qoi_read_u32(const unsigned char* b, unsigned int* o) {
    unsigned int v =
        ((unsigned int)b[*o + 0] << 24) |
        ((unsigned int)b[*o + 1] << 16) |
        ((unsigned int)b[*o + 2] << 8) |
        ((unsigned int)b[*o + 3]);
    *o += 4;
    return v;
}

static inline qoi_error_t qoi_encode(
    const unsigned int* input_rgba,     // Pixel Data
    const unsigned int input_width,     // Array Width
    const unsigned int input_height,    // Array Height
    unsigned char** complete_buffer,    // Output Pointer
    unsigned int* complete_length       // Output Size
) {
    if (!input_rgba || !complete_buffer || !complete_length) {
        return QOI_INVALID_ARGUMENTS;
    }
    unsigned int input_offset = 0;

    // Initialize Encoder
    // We naively assume that everything will into this buffer, if it doesn't then we'll just simply die! :3
    // This function *really* shouldn't be called during gameplay...
    unsigned int output_offset = 0;
    unsigned char* output_buffer = malloc(QOI_MINIMUM_SIZE + (input_width * input_height * 2));
    if (!output_buffer) {
        return QOI_MEMORY_ERROR;
    }

    // Write Header
    output_buffer[output_offset++] = 'q';
    output_buffer[output_offset++] = 'o';
    output_buffer[output_offset++] = 'i';
    output_buffer[output_offset++] = 'f';
    output_buffer[output_offset++] = (input_width >> 24) & 0xFF;
    output_buffer[output_offset++] = (input_width >> 16) & 0xFF;
    output_buffer[output_offset++] = (input_width >> 8) & 0xFF;
    output_buffer[output_offset++] = (input_width) & 0xFF;
    output_buffer[output_offset++] = (input_height >> 24) & 0xFF;
    output_buffer[output_offset++] = (input_height >> 16) & 0xFF;
    output_buffer[output_offset++] = (input_height >> 8) & 0xFF;
    output_buffer[output_offset++] = (input_height) & 0xFF;
    output_buffer[output_offset++] = 0x04; // RGBA
    output_buffer[output_offset++] = 0x00; // sRGB with Linear Alpha

    // Write Content
    unsigned int table[64] = { 0 }; // Color Table
    unsigned int pre = 0x000000FF;  // Previous Pixel
    unsigned int cur = 0x00000000;  // Current Pixel
    unsigned char run = 0;          // Current Run-Length

    for (unsigned int y = 0; y < input_height; y++) {
        for (unsigned int x = 0; x < input_width; x++) {
            cur = input_rgba[input_offset++];

            // Run-Length Check
            if (pre == cur) {
                run++;
                if (run == 62) {
                    // Will Overflow! Flush current run...
                    unsigned char run_byte = QOI_OP_RUN | (run - 1);
                    output_buffer[output_offset++] = run_byte;
                    run = 0;
                }
                continue;
            }
            else if (run) {
                // Pixel Changed, flush current run...
                unsigned char run_byte = QOI_OP_RUN | (run - 1);
                output_buffer[output_offset++] = run_byte;
                run = 0;
            }

            // Opcode: Index
            unsigned char index = (
                ((cur >> 24) & 0xFF) * 3 +
                ((cur >> 16) & 0xFF) * 5 +
                ((cur >> 8) & 0xFF) * 7 +
                ((cur) & 0xFF) * 11
                ) & (64 - 1);
            if (table[index] == cur) {
                pre = cur;
                unsigned char idx_byte = QOI_OP_INDEX | index;
                output_buffer[output_offset++] = idx_byte;
                continue;
            }
            table[index] = cur;

            // Encode Difference
            char dr = ((cur >> 24) & 0xFF) - ((pre >> 24) & 0xFF);
            char dg = ((cur >> 16) & 0xFF) - ((pre >> 16) & 0xFF);
            char db = ((cur >> 8) & 0xFF) - ((pre >> 8) & 0xFF);
            char da = (pre & 0xFF) == (cur & 0xFF);

            if (da && dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                // Opcode: Difference
                unsigned char dif_byte = QOI_OP_DIFF | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
                output_buffer[output_offset++] = dif_byte;
            }
            else if (da && dg >= -32 && dg <= 31 && (dr - dg) >= -8 && (dr - dg) <= 7 && (db - dg) >= -8 && (db - dg) <= 7) {
                // Opcode: Luma
                unsigned char luma_0 = QOI_OP_LUMA | (dg + 32);
                unsigned char luma_1 = ((dr - dg + 8) << 4) | (db - dg + 8);
                output_buffer[output_offset++] = luma_0;
                output_buffer[output_offset++] = luma_1;
            }
            else if (da) {
                // Opcode: RGB
                unsigned char rgb_0 = QOI_OP_RGB;
                unsigned char rgb_1 = (cur >> 24) & 0xFF;
                unsigned char rgb_2 = (cur >> 16) & 0xFF;
                unsigned char rgb_3 = (cur >> 8) & 0xFF;
                output_buffer[output_offset++] = rgb_0;
                output_buffer[output_offset++] = rgb_1;
                output_buffer[output_offset++] = rgb_2;
                output_buffer[output_offset++] = rgb_3;
            }
            else {
                // Opcode: RGBA
                unsigned char rgba_0 = QOI_OP_RGBA;
                unsigned char rgba_1 = (cur >> 24) & 0xFF;
                unsigned char rgba_2 = (cur >> 16) & 0xFF;
                unsigned char rgba_3 = (cur >> 8) & 0xFF;
                unsigned char rgba_4 = (cur) & 0xFF;

                output_buffer[output_offset++] = rgba_0;
                output_buffer[output_offset++] = rgba_1;
                output_buffer[output_offset++] = rgba_2;
                output_buffer[output_offset++] = rgba_3;
                output_buffer[output_offset++] = rgba_4;
            }
            pre = cur;
        }
    }

    // Write Footer
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x00;
    output_buffer[output_offset++] = 0x01;

    unsigned char* resized_buffer = realloc(output_buffer, output_offset);
    if (!resized_buffer) {
        free(output_buffer);
        return QOI_MEMORY_ERROR;
    }

    *complete_length = output_offset;
    *complete_buffer = resized_buffer;
    return QOI_OK;
}

static inline qoi_error_t qoi_decode(
    const unsigned char* input_buffer,  // Encoded Buffer
    const unsigned int input_length,    // Encoded Buffer Length
    unsigned int** image_rgba,          // Pixel Data
    unsigned int* image_width,          // Array Width
    unsigned int* image_height          // Array Height
) {
    if (!input_buffer || !image_rgba || !image_width || !image_height) {
        return QOI_INVALID_ARGUMENTS;
    }
    if (input_length < QOI_MINIMUM_SIZE) {
        return QOI_UNEXPECTED_EOF;
    }
    unsigned int input_offset = 0;

    // Decode Header
    unsigned int header_magic = qoi_read_u32(input_buffer, &input_offset);
    unsigned int header_width = qoi_read_u32(input_buffer, &input_offset);
    unsigned int header_height = qoi_read_u32(input_buffer, &input_offset);
    unsigned char header_channels = input_buffer[input_offset++];
    unsigned char header_colorspace = input_buffer[input_offset++];

    if (header_magic != MAGIC_QOIF) {
        return QOI_NOT_A_QOI_FILE;
    }
    if (
        header_width == 0 || header_width > QOI_MAX_DIMENSION ||
        header_height == 0 || header_height > QOI_MAX_DIMENSION) {
        return QOI_INVALID_HEADER;
    }
    if (
        (header_channels != 3 && header_channels != 4) ||
        header_colorspace > 1
        ) {
        return QOI_INVALID_COLORSPACE;
    }

    // Initialize Decoder
    unsigned char r = 0, g = 0, b = 0, a = 255;
    unsigned int pixel_len = header_width * header_height;
    unsigned int pixel_pos = 0;
    unsigned int pixel_run = 0;
    unsigned int table[64] = { 0 };

    unsigned int output_offset = 0;
    unsigned int* output_buffer = malloc(pixel_len * sizeof(unsigned int));
    if (!output_buffer) {
        return QOI_MEMORY_ERROR;
    }

    while (pixel_pos < pixel_len) {
        if (pixel_run > 0) {
            pixel_run--;
        }
        else {
            if (input_offset > input_length) break;
            unsigned char op = input_buffer[input_offset++];

            if (op == QOI_OP_RGB) {
                if (input_offset + 3 > input_length) break;
                r = input_buffer[input_offset++];
                g = input_buffer[input_offset++];
                b = input_buffer[input_offset++];
            }
            else if (op == QOI_OP_RGBA) {
                if (input_offset + 4 > input_length) break;
                r = input_buffer[input_offset++];
                g = input_buffer[input_offset++];
                b = input_buffer[input_offset++];
                a = input_buffer[input_offset++];
            }
            else if ((op & QOI_OP_MASK) == QOI_OP_INDEX) {
                unsigned int px = table[op];
                r = (px >> 24) & 0xFF;
                g = (px >> 16) & 0xFF;
                b = (px >> 8) & 0xFF;
                a = (px) & 0xFF;
            }
            else if ((op & QOI_OP_MASK) == QOI_OP_DIFF) {
                r += ((op >> 4) & 0x03) - 2;
                g += ((op >> 2) & 0x03) - 2;
                b += (op & 0x03) - 2;
            }
            else if ((op & QOI_OP_MASK) == QOI_OP_LUMA) {
                if (input_offset + 1 > input_length) break;
                unsigned char op2 = input_buffer[input_offset++];
                char vg = (op & 0x3F) - 32;
                r += vg - 8 + ((op2 >> 4) & 0x0F);
                g += vg;
                b += vg - 8 + (op2 & 0x0F);
            }
            else if ((op & QOI_OP_MASK) == QOI_OP_RUN) {
                pixel_run = (op & 0x3F);
            }

            // Update Table
            unsigned char idx = (r * 3 + g * 5 + b * 7 + a * 11) & (64 - 1);
            table[idx] = (r << 24) | (g << 16) | (b << 8) | a;
        }

        output_buffer[output_offset++] = (r << 24) | (g << 16) | (b << 8) | a;
        pixel_pos++;
    }
    if ((pixel_pos < pixel_len) || (input_offset + QOI_FOOTER_SIZE > input_length)) {
        free(output_buffer);
        return QOI_UNEXPECTED_EOF;
    }

    *image_rgba = output_buffer;
    *image_width = header_width;
    *image_height = header_height;
    return QOI_OK;
}