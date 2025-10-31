#pragma once

#define YURI_NAME_LIMIT 1024
#define YURI_LIST_LIMIT 256

#define YURI_FLAG_COMPRESSED    0x80
#define YURI_FLAG_UNASSIGNED_2  0x40
#define YURI_FLAG_UNASSIGNED_3  0x20
#define YURI_FLAG_UNASSIGNED_4  0x10
#define YURI_FLAG_UNASSIGNED_5  0x08
#define YURI_FLAG_UNASSIGNED_6  0x04
#define YURI_FLAG_UNASSIGNED_7  0x02
#define YURI_FLAG_UNASSIGNED_8  0x01

typedef enum {
    YURI_TYPE_EMBEDDED = 1u,
    YURI_TYPE_SHADER_VERTEX = 2u,
    YURI_TYPE_SHADER_FRAGMENT = 3u,
    YURI_TYPE_IMAGE = 4u,
    YURI_TYPE_AUDIO = 5u,
    YURI_TYPE_MODEL = 6u,
    YURI_TYPE_SCENE = 7u,
    YURI_TYPE_SCRIPT = 8u,
    YURI_TYPE_IMAGE_ENCODED = 200u,
    YURI_TYPE_AUDIO_ENCODED = 201u,
} yuri_type_t;

typedef struct {
    char* name;             // Asset Name
    unsigned char type;     // Asset Type
    unsigned char flag;     // Asset Flags
    unsigned int  size;     // Asset Size
    unsigned int  hash;     // Asset Hash
    unsigned int offset;    // Binary Offset
    unsigned char* data;    // Binary Data
} yuri_asset_t;

static const unsigned int MAGIC_YURI = ('Y') | ('U' << 8) | ('R' << 16) | ('I' << 24);

static inline const char* str_type(unsigned char type) {
    switch (type) {
    case YURI_TYPE_EMBEDDED:        return "EMBEDDED";
    case YURI_TYPE_SHADER_VERTEX:   return "SHADER-V";
    case YURI_TYPE_SHADER_FRAGMENT: return "SHADER-F";
    case YURI_TYPE_IMAGE:           return "IMAGE";
    case YURI_TYPE_AUDIO:           return "AUDIO";
    case YURI_TYPE_MODEL:           return "MODEL";
    case YURI_TYPE_SCENE:           return "SCENE";
    case YURI_TYPE_SCRIPT:          return "SCRIPT";
    default: return "N/A";
    }
}