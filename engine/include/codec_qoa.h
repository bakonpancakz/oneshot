#include <stdlib.h>
#include <string.h>
#pragma once

static const unsigned int MAGIC_QOAF = ('q' << 24) | ('o' << 16) | ('a' << 8) | ('f');

typedef enum {
    QOA_OK = 0,
    QOA_MEMORY_ERROR = 1,
    QOA_INVALID_ARGUMENTS = 2,
    QOA_UNEXPECTED_EOF = 3,
    QOA_NOT_A_QOA_FILE = 101,
    QOA_UNSUPPORTED_STREAMING = 102,
    QOA_TOO_MANY_CHANNELS = 103,
    QOA_HEADER_MISMATCH = 104,
    QOA_MALFORMED_FRAME = 105
} qoa_error_t;

#define QOA_LMS_LEN 4
#define QOA_SLICE_LEN 20
#define QOA_MAX_CHANNELS 8
#define QOA_SLICES_PER_FRAME 256
#define QOA_FRAME_LEN (QOA_SLICES_PER_FRAME * QOA_SLICE_LEN)
#define QOA_FRAME_SIZE(channels, slices) \
	(8 + QOA_LMS_LEN * 4 * channels + 8 * slices * channels)

static const int qoa_dequant_tab[16][8] = {
    {   1,    -1,    3,    -3,    5,    -5,     7,     -7},
    {   5,    -5,   18,   -18,   32,   -32,    49,    -49},
    {  16,   -16,   53,   -53,   95,   -95,   147,   -147},
    {  34,   -34,  113,  -113,  203,  -203,   315,   -315},
    {  63,   -63,  210,  -210,  378,  -378,   588,   -588},
    { 104,  -104,  345,  -345,  621,  -621,   966,   -966},
    { 158,  -158,  528,  -528,  950,  -950,  1477,  -1477},
    { 228,  -228,  760,  -760, 1368, -1368,  2128,  -2128},
    { 316,  -316, 1053, -1053, 1895, -1895,  2947,  -2947},
    { 422,  -422, 1405, -1405, 2529, -2529,  3934,  -3934},
    { 548,  -548, 1828, -1828, 3290, -3290,  5117,  -5117},
    { 696,  -696, 2320, -2320, 4176, -4176,  6496,  -6496},
    { 868,  -868, 2893, -2893, 5207, -5207,  8099,  -8099},
    {1064, -1064, 3548, -3548, 6386, -6386,  9933,  -9933},
    {1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005},
    {1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336},
};

static const int qoa_quant_tab[17] = {
    7, 7, 7, 5, 5, 3, 3, 1, /* -8..-1 */
    0,                      /*  0     */
    0, 2, 2, 4, 4, 6, 6, 6  /*  1.. 8 */
};

static const int qoa_reciprocal_tab[16] = {
    65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32
};

static inline int qoa_div(const int v, const int scalefactor) {
    int reciprocal = qoa_reciprocal_tab[scalefactor];
    int n = (v * reciprocal + (1 << 15)) >> 16;
    n = n + ((v > 0) - (v < 0)) - ((n > 0) - (n < 0)); /* round away from 0 */
    return n;
}

static inline int qoa_clamp(const int v, const int min, const int max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static inline int qoa_clamp_s16(const int v) {
    if ((unsigned int)(v + 32768) > 65535) {
        if (v < -32768) { return -32768; }
        if (v > 32767) { return  32767; }
    }
    return v;
}

static inline void qoa_lms_update(
    int weights[QOA_LMS_LEN],
    int history[QOA_LMS_LEN],
    const signed short sample,
    const signed short residual
) {
    int delta = residual >> 4;
    for (unsigned int i = 0; i < QOA_LMS_LEN; i++) {
        weights[i] += history[i] < 0 ? -delta : delta;
    }
    for (unsigned int i = 0; i < QOA_LMS_LEN - 1; i++) {
        history[i] = history[i + 1];
    }
    history[QOA_LMS_LEN - 1] = sample;
}

static inline int qoa_lms_predict(
    int weights[QOA_LMS_LEN],
    int history[QOA_LMS_LEN]
) {
    int prediction = 0;
    for (int i = 0; i < QOA_LMS_LEN; i++) {
        prediction += weights[i] * history[i];
    }
    return prediction >> 13;
}

static inline unsigned int qoa_read_u32(const unsigned char* b, unsigned int* o) {
    unsigned int v =
        ((unsigned int)b[*o + 0] << 24) |
        ((unsigned int)b[*o + 1] << 16) |
        ((unsigned int)b[*o + 2] << 8) |
        ((unsigned int)b[*o + 3]);
    *o += 4;
    return v;
}

static inline unsigned long long qoa_read_u64(const unsigned char* b, unsigned int* o) {
    unsigned long long v =
        ((unsigned long long)b[*o + 0] << 56) |
        ((unsigned long long)b[*o + 1] << 48) |
        ((unsigned long long)b[*o + 2] << 40) |
        ((unsigned long long)b[*o + 3] << 32) |
        ((unsigned long long)b[*o + 4] << 24) |
        ((unsigned long long)b[*o + 5] << 16) |
        ((unsigned long long)b[*o + 6] << 8) |
        ((unsigned long long)b[*o + 7]);
    *o += 8;
    return v;
}

static inline qoa_error_t qoa_decode(
    const unsigned char* input_buffer,  // Encoded Buffer
    const unsigned int input_length,    // Encoded Buffer Length
    signed short** audio_pcm,           // PCM Samples
    unsigned int* audio_samples,        // PCM Samples per Channel
    unsigned int* audio_channels,       // Audio Channels
    unsigned int* audio_sample_rate     // Audio Sample Rate
) {
    if (!input_buffer || !audio_pcm || !audio_samples || !audio_channels || !audio_sample_rate) {
        return QOA_INVALID_ARGUMENTS;
    }
    if (input_length < 16) {
        return QOA_UNEXPECTED_EOF;
    }
    unsigned long long temp64;
    unsigned int temp32;
    unsigned int input_offset = 0;

    // Decode Header
    unsigned int header_magic = qoa_read_u32(input_buffer, &input_offset);
    unsigned int header_samples = qoa_read_u32(input_buffer, &input_offset);
    if (header_magic != MAGIC_QOAF) {
        return QOA_NOT_A_QOA_FILE;
    }
    if (header_samples == 0) {
        // This decoder doesn't have streaming implemented
        return QOA_UNSUPPORTED_STREAMING;
    }

    // Probe First Frame
    if (input_offset + 8 > input_length) {
        return QOA_UNEXPECTED_EOF;
    }
    temp32 = qoa_read_u32(input_buffer, &input_offset);
    unsigned int expect_channels = (temp32 >> 24) & 0xFF;
    unsigned int expect_samplerate = temp32 & 0xFFFFFF;
    input_offset -= 4;
    if (expect_channels > QOA_MAX_CHANNELS) {
        return QOA_TOO_MANY_CHANNELS;
    }

    // Estimate Output Buffer
    unsigned int output_offset = 0;
    unsigned int output_samples = header_samples * expect_channels;
    signed short* output_buffer = malloc(output_samples * sizeof(signed short));
    if (output_buffer == NULL) {
        return QOA_MEMORY_ERROR;
    }
    while (input_offset < input_length && output_offset < output_samples) {

        // Read Frame Header
        if (input_offset + 8 > input_length) {
            // Ensure we have enough bytes to read frame header
            break;
        }
        temp64 = qoa_read_u64(input_buffer, &input_offset);
        unsigned int frame_channels = (temp64 >> 56) & 0xFF;
        unsigned int frame_samplerate = (temp64 >> 32) & 0xFFFFFF;
        unsigned int frame_samples = (temp64 >> 16) & 0xFFFF;
        unsigned int frame_size = temp64 & 0xFFFF;

        // Sanity Checks
        unsigned int frame_length = (frame_size - 8) - (QOA_LMS_LEN * 4 * frame_channels);
        unsigned int frame_slices = frame_length / 8;
        unsigned int frame_samples_max = frame_slices * QOA_SLICE_LEN;

        if (frame_channels > QOA_MAX_CHANNELS) {
            free(output_buffer);
            return QOA_TOO_MANY_CHANNELS;
        }
        if (frame_channels != expect_channels || frame_samplerate != expect_samplerate) {
            free(output_buffer);
            return QOA_HEADER_MISMATCH;
        }
        if (frame_samples * frame_channels > frame_samples_max) {
            free(output_buffer);
            return QOA_MALFORMED_FRAME;
        }
        if (input_offset + frame_length > input_length) {
            free(output_buffer);
            return QOA_UNEXPECTED_EOF;
        }

        // Read Frame LMS State
        int frame_lms_history[QOA_MAX_CHANNELS][QOA_LMS_LEN];
        int frame_lms_weights[QOA_MAX_CHANNELS][QOA_LMS_LEN];
        for (unsigned int c = 0; c < frame_channels; c++) {
            temp64 = qoa_read_u64(input_buffer, &input_offset);
            frame_lms_history[c][0] = (signed short)(temp64 >> 48);
            frame_lms_history[c][1] = (signed short)(temp64 >> 32);
            frame_lms_history[c][2] = (signed short)(temp64 >> 16);
            frame_lms_history[c][3] = (signed short)(temp64);

            temp64 = qoa_read_u64(input_buffer, &input_offset);
            frame_lms_weights[c][0] = (signed short)(temp64 >> 48);
            frame_lms_weights[c][1] = (signed short)(temp64 >> 32);
            frame_lms_weights[c][2] = (signed short)(temp64 >> 16);
            frame_lms_weights[c][3] = (signed short)(temp64);
        }

        // Decode Frame Slices
        for (unsigned int sample_index = 0; sample_index < frame_samples; sample_index += QOA_SLICE_LEN) {
            for (unsigned int c = 0; c < frame_channels; c++) {
                temp64 = qoa_read_u64(input_buffer, &input_offset);

                int scalefactor = (temp64 >> 60) & 0xF;
                temp64 <<= 4;
                int slice_start = sample_index * frame_channels + c;
                int slice_end = qoa_clamp(sample_index + QOA_SLICE_LEN, 0, frame_samples) * frame_channels + c;

                for (int si = slice_start; si < slice_end; si += frame_channels) {
                    int predicted = qoa_lms_predict(frame_lms_weights[c], frame_lms_history[c]);
                    int quantized = (temp64 >> 61) & 0x7;
                    int dequantized = qoa_dequant_tab[scalefactor][quantized];
                    int reconstructed = qoa_clamp_s16(predicted + dequantized);

                    output_buffer[output_offset + si] = (signed short)reconstructed;
                    temp64 <<= 3;

                    qoa_lms_update(
                        frame_lms_weights[c], frame_lms_history[c],
                        reconstructed, dequantized
                    );
                }
            }
        }
        output_offset += frame_samples * frame_channels;
    }
    if (output_offset < output_samples || input_offset < input_length) {
        free(output_buffer);
        return QOA_UNEXPECTED_EOF;
    }

    *audio_samples = output_offset / expect_channels;
    *audio_pcm = output_buffer;
    *audio_channels = expect_channels;
    *audio_sample_rate = expect_samplerate;
    return QOA_OK;
}

static inline qoa_error_t qoa_encode(
    const signed short* audio_pcm,          // PCM Samples
    const unsigned int audio_samples,       // PCM Samples per Channel
    const unsigned int audio_channels,      // Audio Channels
    const unsigned int audio_sample_rate,   // Audio Sample Rate
    unsigned char** complete_buffer,        // Encoded Buffer
    unsigned int* complete_length           // Encoded Buffer Length
) {
    unsigned int input_offset = 0;
    if (!audio_pcm || !complete_buffer || !complete_length) {
        return QOA_INVALID_ARGUMENTS;
    }

    // Initialize Encoder
    unsigned int num_frames = (audio_samples + QOA_FRAME_LEN - 1) / QOA_FRAME_LEN;
    unsigned int num_slices = (audio_samples + QOA_SLICE_LEN - 1) / QOA_SLICE_LEN;
    unsigned int output_offset = 0;
    unsigned int output_length = 8 +                    /* File Header */
        num_frames * 8 +                               	/* Frame Header */
        num_frames * QOA_LMS_LEN * 4 * audio_channels + /* Frame LMS State */
        num_slices * 8 * audio_channels;                	/* Frame Slices */

    unsigned char* output = malloc(output_length);
    if (output == NULL) {
        return QOA_MEMORY_ERROR;
    }

    int lms_weights[QOA_MAX_CHANNELS][QOA_LMS_LEN];
    int lms_history[QOA_MAX_CHANNELS][QOA_LMS_LEN];
    for (unsigned int i = 0; i < audio_channels; i++) {
        // Reference: Magic values help with predictions for start of audio
        lms_weights[i][0] = 0;
        lms_weights[i][1] = 0;
        lms_weights[i][2] = -8192;
        lms_weights[i][3] = 16384;
        // Reference: Clear possible garbage values
        for (unsigned int j = 0; j < QOA_LMS_LEN; j++) {
            lms_history[i][j] = 0;
        }
    }

    // Write Header
    output[output_offset++] = 'q';
    output[output_offset++] = 'o';
    output[output_offset++] = 'a';
    output[output_offset++] = 'f';
    output[output_offset++] = (audio_samples >> 24) & 0xFF;
    output[output_offset++] = (audio_samples >> 16) & 0xFF;
    output[output_offset++] = (audio_samples >> 8) & 0xFF;
    output[output_offset++] = (audio_samples) & 0xFF;

    // Write Frames
    unsigned int frame_length = QOA_FRAME_LEN;
    for (unsigned int frame_index = 0; frame_index < audio_samples; frame_index += frame_length) {
        frame_length = qoa_clamp(QOA_FRAME_LEN, 0, audio_samples - frame_index);

        unsigned int slices = (frame_length + QOA_SLICE_LEN - 1) / QOA_SLICE_LEN;
        unsigned int frame_size = QOA_FRAME_SIZE(audio_channels, slices);
        unsigned int prev_scalefactor[QOA_MAX_CHANNELS];

        // Write Frame Header
        output[output_offset++] = (audio_channels) & 0xFF;
        output[output_offset++] = (audio_sample_rate >> 16) & 0xFF;
        output[output_offset++] = (audio_sample_rate >> 8) & 0xFF;
        output[output_offset++] = (audio_sample_rate) & 0xFF;
        output[output_offset++] = (frame_length >> 8) & 0xFF;
        output[output_offset++] = (frame_length) & 0xFF;
        output[output_offset++] = (frame_size >> 8) & 0xFF;
        output[output_offset++] = (frame_size) & 0xFF;
        for (unsigned int c = 0; c < audio_channels; c++) {

            unsigned long long history = 0, weights = 0;
            for (int i = 0; i < QOA_LMS_LEN; i++) {
                history = (history << 16) | (lms_history[c][i] & 0xFFFF);
                weights = (weights << 16) | (lms_weights[c][i] & 0xFFFF);
            }

            // Write history as 8 bytes (big-endian)
            output[output_offset++] = (history >> 56) & 0xFF;
            output[output_offset++] = (history >> 48) & 0xFF;
            output[output_offset++] = (history >> 40) & 0xFF;
            output[output_offset++] = (history >> 32) & 0xFF;
            output[output_offset++] = (history >> 24) & 0xFF;
            output[output_offset++] = (history >> 16) & 0xFF;
            output[output_offset++] = (history >> 8) & 0xFF;
            output[output_offset++] = (history) & 0xFF;

            // Write weights as 8 bytes (big-endian)
            output[output_offset++] = (weights >> 56) & 0xFF;
            output[output_offset++] = (weights >> 48) & 0xFF;
            output[output_offset++] = (weights >> 40) & 0xFF;
            output[output_offset++] = (weights >> 32) & 0xFF;
            output[output_offset++] = (weights >> 24) & 0xFF;
            output[output_offset++] = (weights >> 16) & 0xFF;
            output[output_offset++] = (weights >> 8) & 0xFF;
            output[output_offset++] = (weights) & 0xFF;
        }

        // Write Frame Samples
        for (unsigned int sample_index = 0; sample_index < frame_length; sample_index += QOA_SLICE_LEN) {
            for (unsigned int c = 0; c < audio_channels; c++) {
                unsigned int slice_length = qoa_clamp(QOA_SLICE_LEN, 0, frame_length - sample_index);
                unsigned int slice_start = sample_index * audio_channels + c;
                unsigned int slice_end = (sample_index + slice_length) * audio_channels + c;

                int best_lms_history[QOA_LMS_LEN] = { 0 };
                int best_lms_weights[QOA_LMS_LEN] = { 0 };
                unsigned int best_scalefactor = 0;
                unsigned long long best_slice = 0;
                unsigned long long best_rank = ~0ULL;

                // Iterate over all possible Scalefactors to find the best one
                for (unsigned int sfi = 0; sfi < 16; sfi++) {
                    unsigned int cur_scalefactor = (sfi + prev_scalefactor[c]) & (16 - 1);
                    unsigned long long cur_slice = cur_scalefactor;
                    unsigned long long cur_rank = 0;

                    int cur_lms_history[QOA_LMS_LEN];
                    int cur_lms_weights[QOA_LMS_LEN];
                    memcpy(cur_lms_history, lms_history[c], sizeof(cur_lms_history));
                    memcpy(cur_lms_weights, lms_weights[c], sizeof(cur_lms_weights));

                    for (unsigned int si = slice_start; si < slice_end; si += audio_channels) {
                        unsigned int sample_offset = input_offset +
                            ((frame_index + sample_index + (si - slice_start) / audio_channels) * audio_channels + c);
                        short sample = *(audio_pcm + sample_offset);

                        int predict = qoa_lms_predict(cur_lms_weights, cur_lms_history);
                        int residual = sample - predict;
                        int scaled = qoa_div(residual, cur_scalefactor);
                        int clamped = qoa_clamp(scaled, -8, 8);
                        int quantized = qoa_quant_tab[clamped + 8];
                        int dequantized = qoa_dequant_tab[cur_scalefactor][quantized];
                        int reconstructed = qoa_clamp_s16(predict + dequantized);

                        // Reference: Prevent pops and clicks by penalizing huge weights
                        int weights_penalty = ((
                            (cur_lms_weights[0] * cur_lms_weights[0]) +
                            (cur_lms_weights[1] * cur_lms_weights[1]) +
                            (cur_lms_weights[2] * cur_lms_weights[2]) +
                            (cur_lms_weights[3] * cur_lms_weights[3])
                            ) >> 18) - 0x8FF;
                        if (weights_penalty < 0) {
                            weights_penalty = 0;
                        }

                        long long error = (sample - reconstructed);
                        cur_rank += (error * error) + (weights_penalty * weights_penalty);
                        if (cur_rank > best_rank) {
                            break;
                        }

                        qoa_lms_update(cur_lms_weights, cur_lms_history, reconstructed, dequantized);
                        cur_slice = (cur_slice << 3) | quantized;
                    }

                    if (cur_rank < best_rank) {
                        best_rank = cur_rank;
                        best_slice = cur_slice;
                        best_scalefactor = cur_scalefactor;
                        memcpy(best_lms_history, cur_lms_history, sizeof(best_lms_history));
                        memcpy(best_lms_weights, cur_lms_weights, sizeof(best_lms_weights));
                    }
                }

                // Update State
                prev_scalefactor[c] = best_scalefactor;
                memcpy(lms_history[c], best_lms_history, sizeof(lms_history[c]));
                memcpy(lms_weights[c], best_lms_weights, sizeof(lms_weights[c]));

                // Write Frame
                // Reference: Left shift all encoded bits to ensure rightmost bits
                // are empty. This should only occur to the last frame of a file.
                best_slice <<= (QOA_SLICE_LEN - slice_length) * 3;
                output[output_offset++] = (best_slice >> 56) & 0xFF;
                output[output_offset++] = (best_slice >> 48) & 0xFF;
                output[output_offset++] = (best_slice >> 40) & 0xFF;
                output[output_offset++] = (best_slice >> 32) & 0xFF;
                output[output_offset++] = (best_slice >> 24) & 0xFF;
                output[output_offset++] = (best_slice >> 16) & 0xFF;
                output[output_offset++] = (best_slice >> 8) & 0xFF;
                output[output_offset++] = (best_slice) & 0xFF;
            }
        }
    }

    *complete_buffer = output;
    *complete_length = output_length;
    return QOA_OK;
}
