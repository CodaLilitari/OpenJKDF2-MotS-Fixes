#ifndef SITH_FLOOR_MISS_CACHE_H
#define SITH_FLOOR_MISS_CACHE_H
#include <stdint.h>
#include <string.h>

// Added: a bounded cache of exact queries whose stock mesh test left all outputs untouched.
#define SITH_FLOOR_MISS_SETS 2048u
#define SITH_FLOOR_MISS_KEYS 4u
typedef struct FloorMissKey
{
    uint32_t ray[8];
    uint32_t flags;
} FloorMissKey;

typedef struct FloorMissSet
{
    uintptr_t model;
    uintptr_t mesh;
    uint64_t revision;
    uint64_t frame;
    unsigned count;
    unsigned next;
    FloorMissKey keys[SITH_FLOOR_MISS_KEYS];
} FloorMissSet;

static FloorMissSet *sithFloorMissCache_GetSet(FloorMissSet *paSets, const void *model, const void *mesh,
                                               uint64_t frame, uint64_t revision)
{
    uintptr_t x = (uintptr_t)mesh;
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    FloorMissSet *set = &paSets[x & (SITH_FLOOR_MISS_SETS - 1)];
    if (set->frame != frame || set->revision != revision || set->model != (uintptr_t)model ||
        set->mesh != (uintptr_t)mesh)
    {
        set->model = (uintptr_t)model;
        set->mesh = (uintptr_t)mesh;
        set->revision = revision;
        set->frame = frame;
        set->count = 0;
        set->next = 0;
    }
    return set;
}

static int sithFloorMissCache_Contains(const FloorMissSet *set, const FloorMissKey *key)
{
    for (unsigned i = 0; i < set->count; ++i)
    {
        if (!memcmp(&set->keys[i], key, sizeof(*key)))
        {
            return 1;
        }
    }
    return 0;
}
static void sithFloorMissCache_Record(FloorMissSet *set, const FloorMissKey *key)
{
    set->keys[set->next] = *key;
    set->next = (set->next + 1) % SITH_FLOOR_MISS_KEYS;
    if (set->count < SITH_FLOOR_MISS_KEYS)
    {
        ++set->count;
    }
}
#endif
