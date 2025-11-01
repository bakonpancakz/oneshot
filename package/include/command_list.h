#include <util_yuri.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

unsigned int read_u32(FILE* f) {
    unsigned char v[4];
    fread(v, sizeof(unsigned char), sizeof(v), f);
    return  (v[3] << 24) | (v[2] << 16) | (v[1] << 8) | (v[0]);
}

unsigned short read_u16(FILE* f) {
    unsigned char v[2];
    fread(v, sizeof(unsigned char), sizeof(v), f);
    return (v[1] << 8) | v[0];
}

unsigned char read_u8(FILE* f) {
    unsigned char v[1];
    fread(v, sizeof(unsigned char), sizeof(v), f);
    return v[0];
}

int command_list(const char* source_file) {
    unsigned int archive_size = 0;
    unsigned int archive_offset = 0;
    unsigned int binary_offset = 0;

    yuri_asset_t asset_list[YURI_LIST_LIMIT];
    unsigned int asset_count = 0;

    // Open File and Get Length
    FILE* f;
    if ((f = fopen(source_file, "rb")) == NULL) {
        printf("Unable to Open File (%s)\n", strerror(errno));
        return 1;
    }
    fseek(f, 0, SEEK_END);
    archive_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("\n* Reading Manifest Entries\n\n");

    // Read Archive Manifest
    if (archive_offset + 8 > archive_size) {
        printf("... : EOF before Archive Header, the archive may be corrupt.\n");
        return 1;
    }
    archive_offset += 8;

    unsigned int header_magic = read_u32(f);
    if (header_magic != MAGIC_YURI) {
        printf("... : Provided File is not a YURI Archive (0x%08X ~= 0x%08X)\n", header_magic, MAGIC_YURI);
        return 1;
    }
    unsigned int header_count = read_u32(f);
    if (header_count > YURI_LIST_LIMIT) {
        printf("... : Provided Archive exceeds capacity of %d item(s)\n", YURI_LIST_LIMIT);
        return 1;
    }

    for (unsigned int i = 0; i < header_count;i++) {
        yuri_asset_t* a = &asset_list[i];

        if (archive_offset + 12 > archive_size) {
            printf("%03d : Reached EOF before Entry was read, the archive may be corrupt.\n", i);
            return 1;
        }
        archive_offset += 12;

        // Read Entry Info
        a->type = read_u8(f);
        a->flag = read_u8(f);
        a->size = read_u32(f);
        a->hash = read_u32(f);
        unsigned short len = read_u16(f);


        if (a->type == 0 || a->type > YURI_TYPE_SCRIPT) {
            printf("%03d : Unknown Asset Type (%d)\n", i, a->type);
            return 1;
        }

        // Read Entry Name
        if (archive_offset + len > archive_size) {
            printf("%03d : Reached EOF before Name was read, the archive may be corrupt.\n", i);
            return 1;
        }
        if ((a->name = malloc(len)) == NULL) {
            printf("%03d : Failed to allocated %d bytes (%s)\n", i, len, strerror(errno));
            return 1;
        }
        fread(a->name, sizeof(char), len, f);
        a->name[len] = '\0';
        a->offset = binary_offset;

        // Counters
        archive_offset += len;
        binary_offset += a->size;
        asset_count++;
        printf("%03d : %-30s . %8s . 0x%08X . 0x%02X . %8.2fKB\n",
            i, a->name, str_type(a->type), a->hash, a->flag, a->size / 1024.00
        );
    }

    printf("\n* Parsed %d Assets from Manifest\n\n", asset_count);

    // Validate Archive Binaries
    for (unsigned int i = 0; i < header_count;i++) {
        yuri_asset_t* a = &asset_list[i];

        // Read Binary
        if ((a->data = malloc(a->size)) == NULL) {
            printf("%03d : Failed to allocated %d bytes (%s)\n", i, a->size, strerror(errno));
            return 1;
        }
        fseek(f, archive_offset + a->offset, SEEK_SET);
        if (fread(a->data, sizeof(unsigned char), a->size, f) != a->size) {
            printf("%03d : Reached EOF before Data was read, the archive may be corrupt.\n", i);
            return 1;
        }

        // Hash Binary
        unsigned int result = crc32(a->data, a->size);
        if (result == a->hash) {
            printf("%03d : Checksum Passed . 0x%08X ^ 0x%08X\n", i, result, a->hash);
        }
        else {
            printf("%03d : Checksum Failed . 0x%08X ^ 0x%08X . The archive may be corrupt.\n", i, result, a->hash);
            return 1;
        }

        // Cleanup
        free(a->data);
        a->data = NULL;
    }

    printf("\n* Archive OK!\n\n");
    fclose(f);

    return 0;
}
