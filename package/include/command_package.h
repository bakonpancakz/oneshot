#include <util_crc32.h>
#include <util_yuri.h>
#include <codec_bmp.h>
#include <codec_wav.h>
#include <codec_qoi.h>

#include <codec_qoa.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>


static inline int str_suffix(const char* string, const char* suffix) {
    size_t string_length = strlen(string);
    size_t suffix_length = strlen(suffix);
    if (suffix_length > string_length) return 0;
    return strcmp(string + string_length - suffix_length, suffix) == 0;
}

int command_package(const char* source_dir, const char* write_path) {
    static yuri_asset_t asset_list[YURI_LIST_LIMIT];
    static int asset_count = 0;
    static char path_base[YURI_NAME_LIMIT];  // Base Directory
    static char path_file[YURI_NAME_LIMIT];  // Path to Subdirectory File

    // Open Archive
    FILE* file_archive;
    if ((file_archive = fopen(write_path, "wb")) == NULL) {
        printf("Cannot Open Output File: %s\n", strerror(errno));
        return 1;
    }

    // Scan Source Directory
    DIR* top_dir = opendir(source_dir);
    struct dirent* top_entry;
    struct stat top_info;
    if (top_dir == NULL) {
        printf("Cannot Open Source Directory: %s\n", strerror(errno));
        return 1;
    }
    while ((top_entry = readdir(top_dir)) != NULL) {
        if (!strcmp(top_entry->d_name, ".") || !strcmp(top_entry->d_name, "..")) continue;

        // Sanity Checks
        snprintf(path_base, sizeof(path_base), "%s/%s", source_dir, top_entry->d_name);
        if (stat(path_base, &top_info) != 0) {
            printf("Cannot Stat Source Entry: %s\n", strerror(errno));
            return 1;
        }
        if (!S_ISDIR(top_info.st_mode)) continue;

        // Scan Source Subdirectory
        DIR* sub_dir = opendir(path_base);
        struct dirent* sub_entry;
        struct stat sub_info;
        if (sub_dir == NULL) {
            printf("Cannot Open Subdirectory: %s\n", strerror(errno));
            return 1;
        }
        while ((sub_entry = readdir(sub_dir)) != NULL) {

            // Sanity Checks
            snprintf(path_file, sizeof(path_file), "%s/%s", path_base, sub_entry->d_name);
            if (stat(path_file, &sub_info) != 0) {
                printf("Cannot Stat Entry: %s\n", strerror(errno));
                return 1;
            }
            if (S_ISDIR(sub_info.st_mode)) continue;

            // Classify Asset
            yuri_asset_t* a = &asset_list[asset_count];
            if (str_suffix(sub_entry->d_name, ".bin"))       a->type = YURI_TYPE_EMBEDDED;
            if (str_suffix(sub_entry->d_name, ".vert.spv"))  a->type = YURI_TYPE_SHADER_VERTEX;
            if (str_suffix(sub_entry->d_name, ".frag.spv"))  a->type = YURI_TYPE_SHADER_FRAGMENT;
            if (str_suffix(sub_entry->d_name, ".bmp"))       a->type = YURI_TYPE_IMAGE;
            if (str_suffix(sub_entry->d_name, ".qoi"))       a->type = YURI_TYPE_IMAGE_ENCODED;
            if (str_suffix(sub_entry->d_name, ".wav"))       a->type = YURI_TYPE_AUDIO;
            if (str_suffix(sub_entry->d_name, ".qoa"))       a->type = YURI_TYPE_AUDIO_ENCODED;
            if (str_suffix(sub_entry->d_name, ".obj"))       a->type = YURI_TYPE_MODEL;
            if (str_suffix(sub_entry->d_name, ".xml"))       a->type = YURI_TYPE_SCENE;
            if (str_suffix(sub_entry->d_name, ".lua"))       a->type = YURI_TYPE_SCRIPT;
            if (a->type == 0) continue;

            // Copy File
            FILE* file_input;
            if ((file_input = fopen(path_file, "rb")) == NULL) {
                printf("Cannot Open File: %s", strerror(errno));
                return 1;
            }
            int file_length = sub_info.st_size;
            unsigned char* file_data = malloc(file_length);
            if (file_data == NULL) {
                printf("Malloc Error: %s", strerror(errno));
                return 1;
            }
            fread(file_data, file_length, 1, file_input);
            fclose(file_input);

            // Process Asset
            switch (a->type) {
            case YURI_TYPE_EMBEDDED:
            case YURI_TYPE_SHADER_VERTEX:
            case YURI_TYPE_SHADER_FRAGMENT:
            case YURI_TYPE_SCENE:
            case YURI_TYPE_SCRIPT:
            case YURI_TYPE_MODEL:
            case YURI_TYPE_AUDIO_ENCODED:
            case YURI_TYPE_IMAGE_ENCODED: {
                // No Encoding
                a->size = file_length;
                a->data = file_data;
                break;
            }
            case YURI_TYPE_IMAGE: {
                unsigned int result = 0, height = 0, width = 0;
                unsigned int* rgba = NULL;

                result = bmp_decode(file_data, file_length, &rgba, &width, &height);
                if (result != BMP_OK) {
                    printf("Unable to decode BMP File (%d)\n", result);
                    return 1;
                }
                free(file_data);

                result = qoi_encode(rgba, width, height, &a->data, &a->size);
                if (result != QOA_OK) {
                    printf("Unable to encode QOI Audio (%d)\n", result);
                    return 1;
                }
                free(rgba);

                break;
            }
            case YURI_TYPE_AUDIO: {
                unsigned int result = 0, channels = 0, rate = 0, samples = 0;
                signed short* pcm = NULL;

                result = wav_decode(file_data, file_length, &pcm, &samples, &channels, &rate);
                if (result != WAV_OK) {
                    printf("Unable to decode WAV File (%d)\n", result);
                    return 1;
                }
                free(file_data);

                result = qoa_encode(pcm, samples, channels, rate, &a->data, &a->size);
                if (result != QOA_OK) {
                    printf("Unable to encode QOA Audio (%d)\n", result);
                    return 1;
                }
                free(pcm);
                break;
            }
            }
            if (a->type == YURI_TYPE_AUDIO_ENCODED) a->type = YURI_TYPE_AUDIO;
            if (a->type == YURI_TYPE_IMAGE_ENCODED) a->type = YURI_TYPE_IMAGE;

            // Copy Filename (Remove Extension)
            static char filename[YURI_NAME_LIMIT];
            strncpy(filename, sub_entry->d_name, sizeof(filename));
            filename[sizeof(filename) - 1] = '\0';
            char* dot = strchr(filename, '.');
            if (dot) {
                *dot = '\0';
            }
            snprintf(path_file, sizeof(path_file), "/%s/%s", top_entry->d_name, filename);
            a->name = strdup(path_file);
            if (a->name == NULL) {
                printf("Cannot Copy String: %s\n", strerror(errno));
                return 1;
            }

            // Process Checksum
            a->hash = crc32(a->data, a->size);
            asset_count++;
            printf(
                "%03d : '%-30s' %8s . 0x%08X . 0x%02X . %8.2fKB\n",
                asset_count, a->name, str_type(a->type), a->hash, a->flag, a->size / 1024.00
            );
        }
    }

    // Write Header
    static unsigned char header[8] = { 'Y','U','R','I' };
    header[4] = (asset_count >> 0) & 0xFF;
    header[5] = (asset_count >> 8) & 0xFF;
    header[6] = (asset_count >> 16) & 0xFF;
    header[7] = (asset_count >> 24) & 0xFF;
    fwrite(header, sizeof(header), 1, file_archive);

    // Write Manifest
    for (int i = 0; i < asset_count; i++) {
        yuri_asset_t* a = &asset_list[i];
        int path_length = strlen(a->name);
        static unsigned char entry[12];
        entry[0] = a->type;
        entry[1] = a->flag;
        entry[2] = (a->size) & 0xFF;
        entry[3] = (a->size >> 8) & 0xFF;
        entry[4] = (a->size >> 16) & 0xFF;
        entry[5] = (a->size >> 24) & 0xFF;
        entry[6] = (a->hash) & 0xFF;
        entry[7] = (a->hash >> 8) & 0xFF;
        entry[8] = (a->hash >> 16) & 0xFF;
        entry[9] = (a->hash >> 24) & 0xFF;
        entry[10] = (path_length >> 0) & 0xFF;
        entry[11] = (path_length >> 8) & 0xFF;
        // Entry & Path
        fwrite(entry, sizeof(entry), 1, file_archive);
        fwrite(a->name, path_length, 1, file_archive);
        free(a->name);
    }

    // Write Files
    for (int i = 0; i < asset_count; i++) {
        yuri_asset_t* a = &asset_list[i];
        fwrite(a->data, a->size, 1, file_archive);
        free(a->data);
    }

    fclose(file_archive);
    return 0;
}
