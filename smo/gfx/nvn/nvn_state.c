// Opaque guest object -> host state. Heap nodes remain stable across growth.
// Main/render/resource-loading threads share the hash index.
#include "nvn_host.h"
#include "nvn_state.h"
#include <pthread.h>

typedef struct state_node {
    nvn_st_ent entry;
    struct state_node *next;
} state_node;
static state_node **buckets;
static unsigned capacity, count, reused;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t resource_serial;
uint64_t nvn_st_resource_serial(void) {
    return __atomic_add_fetch(&resource_serial,1,__ATOMIC_RELAXED);
}

static unsigned st_hash(const void *p, unsigned size) {
    uint64_t h = (uint64_t)(uintptr_t)p;
    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    return (unsigned)h & (size - 1);
}
static void grow(void) {
    unsigned next = capacity ? capacity * 2 : 1024;
    state_node **index = next > capacity ? calloc(next, sizeof *index) : NULL;
    if (!index) { NVN_LOG("NVN state index allocation failed"); abort(); }
    for (unsigned i = 0; i < capacity; i++) {
        state_node *node = buckets[i];
        while (node) {
            state_node *following = node->next;
            unsigned slot = st_hash(node->entry.key, next);
            node->next = index[slot];
            index[slot] = node;
            node = following;
        }
    }
    free(buckets);
    buckets = index;
    capacity = next;
    NVN_LOG("NVN_STATE_CAPACITY buckets=%u objects=%u", capacity, count);
}
static state_node *find(const void *obj) {
    if (!capacity || !obj) return NULL;
    for (state_node *node = buckets[st_hash(obj, capacity)]; node; node = node->next)
        if (node->entry.key == obj) return node;
    return NULL;
}
nvn_st_ent *nvn_st_new(void *obj, uint32_t type) {
    if (!obj) return NULL;
    pthread_mutex_lock(&state_lock);
    state_node *node = find(obj);
    if (node) {
        if (node->entry.type != type) reused++;
    } else {
        if (!capacity || count >= capacity - capacity / 4) grow();
        node = calloc(1, sizeof *node);
        if (!node) { NVN_LOG("NVN state object allocation failed"); abort(); }
        unsigned slot = st_hash(obj, capacity);
        node->next = buckets[slot];
        buckets[slot] = node;
        count++;
    }
    memset(&node->entry, 0, sizeof node->entry);
    node->entry.key = obj;
    node->entry.type = type;
    node->entry.used = 1;
    pthread_mutex_unlock(&state_lock);
    return &node->entry;
}
nvn_st_ent *nvn_st_get(const void *obj, uint32_t type) {
    pthread_mutex_lock(&state_lock);
    state_node *node = find(obj);
    nvn_st_ent *entry = node && node->entry.type == type ? &node->entry : NULL;
    pthread_mutex_unlock(&state_lock);
    return entry;
}
void nvn_st_del(void *obj, uint32_t type) {
    pthread_mutex_lock(&state_lock);
    if (capacity && obj) {
        state_node **link = &buckets[st_hash(obj, capacity)];
        while (*link && (*link)->entry.key != obj) link = &(*link)->next;
        if (*link && (*link)->entry.type == type) {
            state_node *removed = *link;
            *link = removed->next;
            free(removed);
            count--;
        }
    }
    pthread_mutex_unlock(&state_lock);
}
unsigned nvn_st_count(void) {
    pthread_mutex_lock(&state_lock);
    unsigned value = count;
    pthread_mutex_unlock(&state_lock);
    return value;
}
int nvn_st_pool_range(uint64_t address, uint64_t size, uint64_t *base, uint64_t *length) {
    int found = 0;
    pthread_mutex_lock(&state_lock);
    for (unsigned slot = 0; slot < capacity && !found; ++slot) {
        for (state_node *n = buckets[slot]; n; n = n->next) {
            nvn_st_ent *e = &n->entry;
            if (e->type != 4 || !e->v[1] || address < e->v[1]) continue;
            uint64_t offset = address - e->v[1];
            if (offset <= e->v[2] && size <= e->v[2] - offset) {
                *base = e->v[1]; *length = e->v[2]; found = 1; break;
            }
        }
    }
    pthread_mutex_unlock(&state_lock);
    return found;
}
unsigned nvn_st_reused(void) {
    pthread_mutex_lock(&state_lock);
    unsigned value = reused;
    pthread_mutex_unlock(&state_lock);
    return value;
}
void nvn_st_reset(void) {
    pthread_mutex_lock(&state_lock);
    for (unsigned i = 0; i < capacity; i++) {
        state_node *node = buckets[i];
        while (node) { state_node *next = node->next; free(node); node = next; }
    }
    free(buckets);
    buckets = NULL;
    capacity = count = reused = 0;
    pthread_mutex_unlock(&state_lock);
}
// Enumeration requires an unchanged object set, as before.
void *nvn_st_iter(unsigned i, uint32_t *type) {
    pthread_mutex_lock(&state_lock);
    for (unsigned slot = 0; slot < capacity; slot++) {
        for (state_node *node = buckets[slot]; node; node = node->next) {
            if (i) { i--; continue; }
            void *obj = node->entry.key;
            if (type) *type = node->entry.type;
            pthread_mutex_unlock(&state_lock);
            return obj;
        }
    }
    pthread_mutex_unlock(&state_lock);
    return NULL;
}
