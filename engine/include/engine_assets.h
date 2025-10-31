#include <engine_config.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#pragma once

static const unsigned int MAGIC_YURI = ('Y') | ('U' << 8) | ('R' << 16) | ('I' << 24);

#define YURI_SIZE_HEADER \
    (sizeof(unsigned int) * 2)

#define YURI_SIZE_ENTRY  \
    (sizeof(unsigned char) * 2) + (sizeof(unsigned int) * 2) + sizeof(unsigned short)

#define ASSET_ARCHIVE_LIMIT                  32
#define ASSET_REGISTRY_CLEANUP_INTERVAL      30
#define ASSET_REGISTRY_MEMORY_PROBE_INTERVAL 3
#define ASSET_REGISTRY_MEMORY_PRESSURE_LIMIT 80

typedef enum {
    ASSET_TYPE_EMBEDDED = 1u,
    ASSET_TYPE_SHADER_VERTEX = 2u,
    ASSET_TYPE_SHADER_FRAGMENT = 3u,
    ASSET_TYPE_IMAGE = 4u,
    ASSET_TYPE_AUDIO = 5u,
    ASSET_TYPE_MODEL = 6u,
    ASSET_TYPE_SCENE = 7u,
    ASSET_TYPE_SCRIPT = 8u,
} __attribute__((__packed__)) asset_type_t;

typedef enum {
    ASSET_STATE_DISK = 1u,          // Asset is unloaded and can be found on the disk
    ASSET_STATE_WAIT = 2u,          // Asset is awaiting a worker thread to process it
    ASSET_STATE_BUSY = 3u,          // Asset is being processed by a worker thread
    ASSET_STATE_UPLOAD = 4u,        // Asset is sitting in memory waiting for GPU upload
    ASSET_STATE_DONE = 5u           // Asset is ready and meta field is set
} __attribute__((__packed__)) asset_state_t;

typedef struct {
    unsigned char* data;            // Embedded Data
    unsigned int size;              // Embedded Size
} asset_metadata_embedded_t;

typedef struct {
    unsigned char* code;            // Shader Code
    unsigned int size;              // Shader Size
} asset_metadata_shader_t;

typedef struct {
    unsigned int* pixels;           // Image RGBA Pixels
    unsigned int width;             // Image Width
    unsigned int height;            // Image Height
} asset_metadata_image_t;

typedef struct {
    unsigned int samples;           // Samples per Channel
    unsigned int channels;          // Channels
    unsigned int sampleRate;        // Sample Rate
    signed short* pcm;              // PCM Samples
} asset_metadata_audio_t;

typedef union {
    asset_metadata_embedded_t embed;
    asset_metadata_shader_t shader;
    asset_metadata_image_t image;
    asset_metadata_audio_t audio;
} asset_metadata_u;

typedef struct {
    atomic_char state;              // Asset State
    atomic_char type;               // Asset Type
    atomic_uint used;               // Asset Used 
    unsigned char flag;             // Asset Flags
    unsigned int hash;              // Asset Hash
    unsigned int name_length;       // Asset Name Length (Faster Lookups)
    unsigned int archive_id;        // Archive ID
    unsigned int archive_offset;    // Archive Read Offset
    unsigned int archive_length;    // Archive Read Length
    char* name;                     // Asset Name
    asset_metadata_u meta;          // Type Metadata
} asset_t;

typedef struct {
    asset_t* assets;                // Registry Assets
    unsigned int size;              // Registry Size
    unsigned int capacity;          // Registry Capacity
    pthread_mutex_t mtx;            // Registry Mutex
} asset_registry_t;

typedef struct {
    unsigned int id;
} asset_worker_args_t;

static inline const char* asset_str_type(unsigned char type) {
    switch (type) {
    case ASSET_TYPE_EMBEDDED:        return "EMBEDDED";
    case ASSET_TYPE_SHADER_VERTEX:   return "SHADER-V";
    case ASSET_TYPE_SHADER_FRAGMENT: return "SHADER-F";
    case ASSET_TYPE_IMAGE:           return "IMAGE";
    case ASSET_TYPE_AUDIO:           return "AUDIO";
    case ASSET_TYPE_MODEL:           return "MODEL";
    case ASSET_TYPE_SCENE:           return "SCENE";
    case ASSET_TYPE_SCRIPT:          return "SCRIPT";
    default: return "N/A";
    }
}

// Preallocate Space for X more assets in the registry
// - This function is not thread safe and assumes you have manually locked the mutex.
bool_t registry_unsafe_preallocate(asset_registry_t* r, unsigned int amount);

// Open Archive and Parse it's contents, adding them to the registry. 
// - This function is not thread safe and assumes you have manually locked the mutex.
bool_t registry_unsafe_parse(asset_registry_t* r, const char* archive_path, const unsigned int archive_id);

// Discard Asset Metadata from Memory
// - This function is not thread safe and assumes you have manually locked the mutex.
void registry_unsafe_free_meta(asset_t* a);

// Discard Registry Contents from Memory
void registry_free(asset_registry_t* r);

// Releasing memory by freeing unused assets. This assumes that usage it's usage 
// was properly tracked by the asset_acquire() and asset_release() functions.
void registry_collect(asset_registry_t* r);

// Search for an asset using it's name and type. Ensure it is loaded by calling
// assets_acquire() on it. 
asset_t* assets_unsafe_find(const asset_type_t find_type, const char* find_name);

// Mark the asset as required. Ensure that it's state is ASSET_STATE_READY 
// before use. Additionally it cannot garbage collected until assets_release() 
// is called on it
void assets_acquire(asset_t* a);

// Mark the asset as no longer needed, if no other instance is using this it
// can be garbage collected in the future.
void assets_release(asset_t* a);

// [INTERNAL] Thread which handles asset decoding
void* engine_assets_worker(void* data);

// [INTERNAL] Start Subsystem: Assets
bool_t engine_assets_init(const engine_config_t* config);

// [INTERNAL] Tick Subsystem: Assets
bool_t engine_assets_tick(float delta);

// [INTERNAL] Stop Subsystem: Assets
void engine_assets_exit(void);