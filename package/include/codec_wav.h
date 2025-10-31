#include <stdlib.h>
#include <string.h>
#pragma once

static const unsigned int MAGIC_WAV_RIFF = ('R' | 'I' << 8 | 'F' << 16 | 'F' << 24);
static const unsigned int MAGIC_WAV_WAVE = ('W' | 'A' << 8 | 'V' << 16 | 'E' << 24);
static const unsigned int MAGIC_WAV_DATA = ('d' | 'a' << 8 | 't' << 16 | 'a' << 24);
static const unsigned int MAGIC_WAV_FMT = ('f' | 'm' << 8 | 't' << 16 | ' ' << 24);

typedef enum {
    WAV_OK = 0,
    WAV_MEMORY_ERROR = 1,
    WAV_UNEXPECTED_EOF = 3,
    WAV_INVALID_ARGUMENTS = 2,
    WAV_NOT_A_RIFF_FILE = 100,
    WAV_NOT_A_WAVE_FILE = 101,
    WAV_UNSUPPORTED_FMT_CHUNK_SIZE = 102,
    WAV_UNSUPPORTED_FMT_EXTRA_DATA = 103,
    WAV_UNSUPPORTED_AUDIO_FORMAT = 104,
    WAV_UNSUPPORTED_BIT_DEPTH = 105,
    WAV_MISSING_DATA_CHUNK = 106
} wav_error_t;

static inline unsigned short wav_read_u16(const unsigned char* b, unsigned int* o) {
    unsigned short v =
        ((unsigned short)b[*o + 1] << 8) |
        ((unsigned short)b[*o + 0]);
    *o += 2;
    return v;
}

static inline unsigned int wav_read_u32(const unsigned char* b, unsigned int* o) {
    unsigned int v =
        ((unsigned int)b[*o + 3] << 24) |
        ((unsigned int)b[*o + 2] << 16) |
        ((unsigned int)b[*o + 1] << 8) |
        ((unsigned int)b[*o + 0]);
    *o += 4;
    return v;
}

static inline wav_error_t wav_decode(
    const unsigned char* input_buffer,  // Encoded Buffer
    const unsigned int input_length,    // Encoded Buffer Size    
    signed short** audio_pcm,           // PCM Samples
    unsigned int* audio_samples,        // PCM Samples per Channel
    unsigned int* audio_channels,       // Audio Channel
    unsigned int* audio_samplerate      // Audio Sample Rate
) {
    if (!input_buffer || !audio_pcm || !audio_samples || !audio_channels || !audio_samplerate) {
        return WAV_INVALID_ARGUMENTS;
    }
    
    // Read File Header
    unsigned int input_offset = 0;
    unsigned int expect_size = 0, expect_format = 0, expect_samples = 0;
    unsigned int expect_channels = 0, expect_sample_rate = 0, expect_bit_depth = 0;

    if (input_offset + 12 > input_length) {
        return WAV_UNEXPECTED_EOF;
    }
    if (wav_read_u32(input_buffer, &input_offset) != MAGIC_WAV_RIFF) {
        return WAV_NOT_A_RIFF_FILE;
    }
    input_offset += 4; // Ignore File Size
    if (wav_read_u32(input_buffer, &input_offset) != MAGIC_WAV_WAVE) {
        return WAV_NOT_A_WAVE_FILE;
    }

    // Read File Chunks
    // We desire only required the 'fmt ' chunk for out audio format
    // and the 'data' chunk for our PCM data.
    while (1) {
        if (input_offset + 8 > input_length) {
            break;
        }
        unsigned int chunk_type = wav_read_u32(input_buffer, &input_offset);
        unsigned int chunk_size = wav_read_u32(input_buffer, &input_offset);
        if (chunk_type == MAGIC_WAV_FMT) {
            if (chunk_size != 16 && chunk_size != 18) {
                return WAV_UNSUPPORTED_FMT_CHUNK_SIZE;
            }

            // Read format parameters
            if (input_offset + 16 > input_length) {
                return WAV_UNEXPECTED_EOF;
            }
            expect_format /*-----*/ = wav_read_u16(input_buffer, &input_offset);
            expect_channels /*---*/ = wav_read_u16(input_buffer, &input_offset);
            expect_sample_rate /**/ = wav_read_u32(input_buffer, &input_offset);
            input_offset += 6; //     skip byte_rate (u32), block_align (u16)
            expect_bit_depth /*--*/ = wav_read_u16(input_buffer, &input_offset);

            // Check for unsupported extra parameters
            if (chunk_size == 18) {
                if (input_offset + 2 > input_length) {
                    return WAV_UNEXPECTED_EOF;
                }
                if (wav_read_u16(input_buffer, &input_offset)) {
                    return WAV_UNSUPPORTED_FMT_EXTRA_DATA;
                }
            }
            continue;
        }
        if (chunk_type == MAGIC_WAV_DATA) {
            expect_size = chunk_size;
            break;
        }
        if (input_offset + chunk_size > input_length) {
            return WAV_UNEXPECTED_EOF;
        }
        input_offset += chunk_size;
    }
    if (expect_format != 0x01 /* PCM (1) */) {
        return WAV_UNSUPPORTED_AUDIO_FORMAT;
    }
    if (expect_size == 0) {
        return WAV_MISSING_DATA_CHUNK;
    }
    if (expect_bit_depth != 16) {
        return WAV_UNSUPPORTED_BIT_DEPTH;
    }

    // Copy Samples
    if (input_offset + expect_size > input_length) {
        return WAV_UNEXPECTED_EOF;
    }
    signed short* pcm = malloc(expect_size);
    if (!pcm) {
        return WAV_MEMORY_ERROR;
    }
    memcpy(pcm, input_buffer + input_offset, expect_size);
    expect_samples = expect_size / (expect_channels * (expect_bit_depth / 8));

    *audio_pcm = pcm;
    *audio_samples = expect_samples;
    *audio_channels = expect_channels;
    *audio_samplerate = expect_sample_rate;
    return WAV_OK;
}