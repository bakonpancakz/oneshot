#include <engine_assets.h>
#include <engine_logger.h>
#include <util_bytes.h>
#include <stdatomic.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <sysinfoapi.h>
static MEMORYSTATUSEX mem_state;
static float registry_last_cleanup_memory = 0;
#endif

static bool worker_running = TRUE;
static unsigned int worker_count = 0;
static asset_worker_args_t* worker_args;
static pthread_mutex_t worker_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t worker_cond = PTHREAD_COND_INITIALIZER;
static pthread_t* worker_threads;

static char* archive_paths[] = {
    "assets.yuri"
};
static unsigned int archive_count = sizeof(archive_paths) / sizeof(archive_paths[0]);
static float registry_last_cleanup = 0;
static asset_registry_t* registry = NULL;


bool_t registry_unsafe_preallocate(asset_registry_t* r, unsigned int amount) {
    size_t old_capacity = r->capacity;
    asset_t* new_list = realloc(r->assets, (r->capacity + amount) * sizeof(asset_t));
    if (new_list == NULL) {
        return FALSE;
    }
    memset(&new_list[old_capacity], 0, amount * sizeof(asset_t));
    r->assets = new_list;
    r->capacity += amount;
    return TRUE;
}

bool_t registry_unsafe_parse(asset_registry_t* r, const char* archive_path, const unsigned int archive_id) {
    unsigned int archive_size = 0;
    FILE* archive_handle;

    // Open Archive
    if ((archive_handle = fopen(archive_path, "rb")) == NULL) {
        logger(LERROR, OASSET, "Unable to Open Archive (%s)", strerror(errno));
        return FALSE;
    }
    fseek(archive_handle, 0, SEEK_END);
    archive_size = ftell(archive_handle);
    fseek(archive_handle, 0, SEEK_SET);

    // Read YURI Header
    if (YURI_SIZE_HEADER > archive_size) {
        logger(LERROR, OASSET, "Unexpected EOF reading Archive Header");
        fclose(archive_handle);
        return FALSE;
    }
    unsigned int identify = read_u32_le(archive_handle);
    unsigned int entries = read_u32_le(archive_handle);

    if (identify != MAGIC_YURI) {
        logger(LERROR, OASSET, "File is not a YURI Archive");
        fclose(archive_handle);
        return FALSE;
    }
    if (!registry_unsafe_preallocate(r, entries)) {
        logger(LERROR, OASSET, "Failed to Allocate Memory for %d Entries (%s)",
            entries, strerror(errno));
        fclose(archive_handle);
        return FALSE;
    }

    // Read YURI Entries
    unsigned int offset_index = r->size;
    unsigned int offset_binary = 0;
    for (unsigned int i = 0; i < entries; i++) {

        if (ftell(archive_handle) + YURI_SIZE_ENTRY > archive_size) {
            logger(LERROR, OASSET, "Unexpected EOF reading Entry #%d", i);
            fclose(archive_handle);
            return FALSE;
        }
        unsigned char type = read_u8(archive_handle);
        unsigned char flag = read_u8(archive_handle);
        unsigned int size = read_u32_le(archive_handle);
        unsigned int hash = read_u32_le(archive_handle);
        unsigned int len = (unsigned int)read_u16_le(archive_handle);
        char* name = NULL;

        if (type == 0 || type > ASSET_TYPE_SCRIPT) {
            logger(LERROR, OASSET, "Unsupported Asset Type %d (#%d)", i, type);
            fclose(archive_handle);
            return FALSE;
        }

        // Copy Entry Name
        if (ftell(archive_handle) + len > archive_size) {
            logger(LERROR, OASSET,
                "Unexpected EOF reading Entry Name", i);
            fclose(archive_handle);
            return FALSE;
        }
        if ((name = malloc(len)) == NULL) {
            logger(LERROR, OASSET, "Failed to Allocate Memory for Entry Name (#%d)", i);
            fclose(archive_handle);
            return FALSE;
        }
        fread(name, sizeof(unsigned char), len, archive_handle);
        name[len] = '\0';

        // Copy Entry
        asset_t* a = &r->assets[r->size++];
        atomic_store(&a->state, ASSET_STATE_DISK);
        atomic_store(&a->type, type);
        atomic_store(&a->used, 0);
        a->flag = flag;
        a->hash = hash;
        a->name_length = len;
        a->archive_id = archive_id;
        a->archive_offset = offset_binary;
        a->archive_length = size;
        a->name = name;

        // Track Offsets
        offset_binary += size;
        logger(LDEBUG, OASSET,
            "%03d : '%-30s' %8s . 0x%08X . 0x%02X . %8.2fKB",
            i + 1, name, asset_str_type(type), hash, flag, size / 1024.00
        );
    }

    // Adjust Read Offset for Archive Header, otherwise we'll be way off...
    unsigned int offset_header = ftell(archive_handle);
    for (unsigned int i = 0; i < entries; i++) {
        r->assets[offset_index + i].archive_offset += offset_header;
    }

    fclose(archive_handle);
    return TRUE;
}

void registry_unsafe_free_meta(asset_t* a) {

    // TODO: GPU related stuff should push commands to an atomic ring style command
    // buffer so that the render thread can periodically scan and process them.
    // That's my idea at least...

    switch (a->type) {
    case ASSET_TYPE_EMBEDDED: {
        free(a->meta.embed.data);
        a->meta.embed.data = NULL;
        a->meta.embed.size = 0;
        break;
    }
    case ASSET_TYPE_SHADER_VERTEX:
    case ASSET_TYPE_SHADER_FRAGMENT: {
        // TODO: Free from CPU & GPU Memory (from here somehow?)
        break;
    }
    case ASSET_TYPE_IMAGE: {
        // TODO: Free from CPU & GPU Memory (from here somehow?)
        break;
    }
    case ASSET_TYPE_AUDIO: {
        free(a->meta.audio.pcm);
        a->meta.audio.pcm = NULL;
        a->meta.audio.sampleRate = 0;
        a->meta.audio.channels = 0;
        a->meta.audio.samples = 0;
        break;
    }
    case ASSET_TYPE_MODEL: {
        // TODO: Free from CPU & GPU Memory (from here somehow?)
        break;
    }
    case ASSET_TYPE_SCENE: {
        // TODO: IDK WHAT THESE EXACTLY DO YET
        break;
    }
    case ASSET_TYPE_SCRIPT: {
        // TODO: Release from Lua VM (from here somehow?)
        break;
    }
    }
}

void registry_free(asset_registry_t* r) {
    pthread_mutex_lock(&r->mtx);
    if (r->assets) {
        for (unsigned int i = 0; i < r->size; i++) {
            asset_t* a = &r->assets[i];
            if (a->name != NULL) {
                free(a->name);
                a->name = NULL;
            }
            registry_unsafe_free_meta(a);
        }
        free(r->assets);
        r->assets = NULL;
    }
    r->capacity = 0;
    r->size = 0;
    pthread_mutex_unlock(&r->mtx);
    pthread_mutex_destroy(&r->mtx);
}

void registry_collect(asset_registry_t* r) {
    pthread_mutex_lock(&r->mtx);
    for (unsigned int i = 0; i < r->size; i++) {
        asset_t* a = &r->assets[i];
        if ((unsigned int)atomic_load(&a->used) > 0) {
            continue;
        }
        asset_state_t expect = ASSET_STATE_DONE;
        if (atomic_compare_exchange_strong(&a->state, &expect, ASSET_STATE_DISK)) {
            registry_unsafe_free_meta(a);
        }
    }
    pthread_mutex_unlock(&r->mtx);
}

asset_t* assets_unsafe_find(const asset_type_t find_type, const char* find_name) {
    size_t find_length = strlen(find_name);
    for (unsigned int i = 0; i < registry->size; i++) {
        asset_t* a = &registry->assets[i];
        if (a->type != find_type) continue;
        if (a->name_length != find_length) continue;
        if (!strcmp(a->name, find_name)) {
            return a;
        }
    }
    return NULL;
}

void assets_acquire(asset_t* a) {
    atomic_fetch_add(&a->used, 1);
    asset_state_t expect = ASSET_STATE_DISK;
    if (atomic_compare_exchange_strong(&a->state, &expect, ASSET_STATE_WAIT)) {
        pthread_mutex_lock(&worker_mutex);
        pthread_cond_signal(&worker_cond);
        pthread_mutex_unlock(&worker_mutex);
    }
}

void assets_release(asset_t* a) {
    atomic_fetch_sub(&a->used, 1);
}

void* engine_assets_worker(void* data) {
    asset_worker_args_t* args = (asset_worker_args_t*)data;
    logger(LINFO, OASSET, "Worker %02d: Spawned", args->id);

    // Open Archives
    FILE* archive_handles[ASSET_ARCHIVE_LIMIT] = { 0 };
    for (unsigned int i = 0; i < archive_count; i++) {
        if ((archive_handles[i] = fopen(archive_paths[i], "rb")) == NULL) {
            logger(LERROR, OASSET, "Worker %02d: Failed to open archive '%s' (%s)",
                args->id, archive_paths[i], strerror(errno));
            return NULL;
        }
    }

    // Worker Loop
    while (worker_running) {

        // Await Work
        pthread_mutex_lock(&worker_mutex);
        pthread_cond_wait(&worker_cond, &worker_mutex);
        pthread_mutex_unlock(&worker_mutex);
        if (!worker_running) {
            break;
        }

        // TODO: Search For Work
        // TODO: Implement Asset Loading
    }

    logger(LINFO, OASSET, "Worker %02d: Closed", args->id);
    return NULL;
}

bool_t engine_assets_init(const engine_config_t* config) {
    worker_count = config->asset_threads;

    // Performance Warnings
    if (worker_count > ASSET_WORKER_LIMIT) {
        logger(LWARN, OASSET,
            "An excessive amount of worker threads was requested!"
            " Limiting to %d threads.",
            ASSET_WORKER_LIMIT
        );
        worker_count = ASSET_WORKER_LIMIT;
    }
    if (sizeof(asset_t) > ASSET_CACHE_LINE_SIZE) {
        logger(LWARN, OASSET,
            "Struct 'asset_t' is %d bytes overweight!"
            " CPU Cache performance may be affected.",
            sizeof(asset_t) - ASSET_CACHE_LINE_SIZE
        );
    }

    // Initialize Registry
    if ((registry = malloc(sizeof(asset_registry_t))) == NULL) {
        logger(LERROR, OASSET, "Cannot Initialize Asset Registry (%s)", strerror(errno));
        return FALSE;
    }
    memset(registry, 0, sizeof(asset_registry_t));
    pthread_mutex_init(&registry->mtx, NULL);

    // Parse Archives
    for (unsigned int i = 0; i < archive_count; i++) {
        logger(LINFO, OASSET, "Parsing Archive : %s", archive_paths[i]);
        if (!registry_unsafe_parse(registry, archive_paths[i], i)) {
            return FALSE;
        }
    }

    // Create Worker Threads
    logger(LINFO, OASSET, "Creating %d Worker(s)", config->asset_threads);
    if ((worker_threads = malloc(sizeof(pthread_t) * worker_count)) == NULL) {
        logger(LERROR, OASSET, "Memory Error (%s)", strerror(errno));
        return FALSE;
    }
    if ((worker_args = malloc(sizeof(asset_worker_args_t) * worker_count)) == NULL) {
        logger(LERROR, OASSET, "Memory Error (%s)", strerror(errno));
        return FALSE;
    }
    for (unsigned int i = 0; i < worker_count; i++) {
        asset_worker_args_t* args = &worker_args[i];
        args->id = (i + 1);

        pthread_t* thread = &worker_threads[i];
        int error = pthread_create(thread, NULL, engine_assets_worker, args);
        if (error) {
            logger(LERROR, OASSET, "Failed to Create Worker #%02d (%s)", i, strerror(error));
            return FALSE;
        }
    }

    return TRUE;
}

bool_t engine_assets_tick(float delta) {
    registry_last_cleanup += delta;

    // Regular Memory Collection
    if (registry_last_cleanup > ASSET_REGISTRY_CLEANUP_INTERVAL) {
        logger(LWARN, OASSET, "Cleaning Registry...");
        registry_collect(registry);
        registry_last_cleanup -= ASSET_REGISTRY_CLEANUP_INTERVAL;
    }

    // Force Collection if Memory Pressure is High
#ifdef _WIN32
    registry_last_cleanup_memory += delta;
    if (registry_last_cleanup_memory > ASSET_REGISTRY_MEMORY_PROBE_INTERVAL) {
        if (
            GlobalMemoryStatusEx(&mem_state) &&
            mem_state.dwMemoryLoad > ASSET_REGISTRY_MEMORY_PRESSURE_LIMIT
            ) {
            logger(LWARN, OASSET, "Memory Pressure High! Cleaning Registry...");
            registry_collect(registry);
        }
        registry_last_cleanup_memory -= ASSET_REGISTRY_MEMORY_PROBE_INTERVAL;
    }
#endif

    return TRUE;
}

void engine_assets_exit(void) {

    // Cleanup Threads
    pthread_mutex_lock(&worker_mutex);
    worker_running = 0;
    pthread_cond_broadcast(&worker_cond);
    pthread_mutex_unlock(&worker_mutex);
    for (unsigned int i = 0; i < worker_count; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    // Cleanup Registry
    registry_free(registry);
    registry = NULL;
}