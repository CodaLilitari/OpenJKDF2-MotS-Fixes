/* The real plane gate, face/mesh loops and cache dispatch run on a synthetic floor.
 * The fixture supplies vector operations and an XY triangle membership test. */
#include "types.h"
#include "Engine/sithFloorMissCache.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct rdMesh
{
    int numFaces;
    rdFace *faces;
    rdVector3 *aVertices;
};
struct rdGeoset
{
    rdMesh *aMeshes;
};
struct rdModel3
{
    rdGeoset aGeos[1];
};
static FloorMissSet *sithIntersect_paFloorMissSets;
static uint64_t sithIntersect_floorQueryUpdate, rdModel3_geometryRevision;
static unsigned planeTests;
#define SITH_ASSERTREL(condition) assert(condition)
#define _memcpy memcpy
#define rdVector_Copy3(a, b) (*(a) = *(b))
static float rdVector_Dot3(const rdVector3 *a, const rdVector3 *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}
static void rdVector_Sub3(rdVector3 *out, const rdVector3 *a, const rdVector3 *b)
{
    *out = (rdVector3){a->x - b->x, a->y - b->y, a->z - b->z};
}
static void rdVector_Scale3(rdVector3 *out, const rdVector3 *a, float scale)
{
    *out = (rdVector3){a->x * scale, a->y * scale, a->z * scale};
}
static void rdVector_ScaleAdd3Acc(rdVector3 *a, const rdVector3 *b, float scale)
{
    a->x += b->x * scale;
    a->y += b->y * scale;
    a->z += b->z * scale;
}
static void rdVector_Add3Acc(rdVector3 *a, const rdVector3 *b)
{
    rdVector_ScaleAdd3Acc(a, b, 1);
}
static void rdVector_Normalize3Acc(rdVector3 *a)
{
    float n = sqrtf(rdVector_Dot3(a, a));
    if (n)
    {
        rdVector_Scale3(a, a, 1 / n);
    }
}
static float rdMath_DistancePointToPlane(const rdVector3 *a, const rdVector3 *n, const rdVector3 *p)
{
    rdVector3 v;
    rdVector_Sub3(&v, a, p);
    ++planeTests;
    return rdVector_Dot3(&v, n);
}
static float stdMath_ClipNearZero(float x)
{
    return fabsf(x) < 0.00001f ? 0 : x;
}
static int sithIntersect_TestSphereFaceHit(rdVector3 *p, float radius, rdFace *face, rdVector3 *vertices,
                                           int *mask)
{
    assert(radius == 0);
    (void)face;
    (void)vertices;
    *mask = 0;
    return p->x >= 0 && p->y >= 0 && p->x + p->y <= 1;
}
static int sithIntersect_CheckSphereFaceHitVerticesIntersection(rdVector3 *p, float r, rdFace *f,
                                                                rdVector3 *v, int m, rdVector3 *n)
{
    (void)p;
    (void)r;
    (void)f;
    (void)v;
    (void)m;
    (void)n;
    abort(); /* Zero-radius triangle fixtures never enter the edge/sphere branch. */
}
static int failAllocation;
static void *fixtureCalloc(size_t count, size_t size)
{
    if (failAllocation)
        return NULL;
    return calloc(count, size);
}
#define calloc fixtureCalloc
#include "floor_production.inc"
#undef calloc

int main(void)
{
    int indices[] = {0, 1, 2};
    rdVector3 vertices[] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, down = {-0.0f, -0.0f, -1},
              start = {.25f, .25f, .08f};
    rdFace triangle = {.numVertices = 3, .vertexPosIdx = indices, .normal = {0, 0, 1}}, sentinel = {0};
    rdMesh mesh = {1, &triangle, vertices};
    rdModel3 model = {.aGeos = {{&mesh}}};
    int flags = RAYCAST_2000 | RAYCAST_800 | RAYCAST_10 | RAYCAST_2;
    SithThing target = {0};
    target.type = SITH_THING_COG;
    target.flags = SITH_TF_LEVELGEO;
    target.collide = SITH_COLLIDE_FACE;
    target.renderData.type = RD_THING_MODEL3;
    target.renderData.model3 = &model;

    unsetenv("OPENJKDF2_FLOOR_MISS_CACHE");
    sithIntersect_OpenFloorMissCache();
    assert(!sithIntersect_paFloorMissSets);
    setenv("OPENJKDF2_FLOOR_MISS_CACHE", "1", 1);
    failAllocation = 1;
    sithIntersect_OpenFloorMissCache();
    assert(!sithIntersect_paFloorMissSets);
    assert(!sithIntersect_IsCacheableFloorQuery(NULL, &target, flags, &down, 0));
    failAllocation = 0;
    sithIntersect_OpenFloorMissCache();
    assert(sithIntersect_paFloorMissSets);
    assert(sithIntersect_IsCacheableFloorQuery(NULL, &target, flags, &down, 0));
    assert(!sithIntersect_IsCacheableFloorQuery(NULL, &target, flags ^ RAYCAST_10, &down, 0));
    assert(!sithIntersect_IsCacheableFloorQuery(&target, &target, flags, &down, 0));
    assert(!sithIntersect_IsCacheableFloorQuery(NULL, &target, flags, &down, .01f));
    target.moveType = SITH_MT_PHYSICS;
    assert(!sithIntersect_IsCacheableFloorQuery(NULL, &target, flags, &down, 0));

    float distance = 42;
    rdFace *hit = &sentinel;
    rdVector3 normal = {4, 5, 6}, initial = normal;
#define QUERY()                                                                                              \
    sithIntersect_CheckCachedFloorMesh(&model, &mesh, &start, &down, .073f, 0, flags, &distance, &hit,       \
                                       &normal)
    assert(!QUERY());
    unsigned originalTests = planeTests;
    assert(originalTests > 0);
    assert(!QUERY() && planeTests == originalTests);
    assert(distance == 42 && hit == &sentinel && !memcmp(&initial, &normal, sizeof normal));
    start.z = nextafterf(start.z, INFINITY);
    assert(!QUERY() && planeTests > originalTests);
    originalTests = planeTests;
    sithIntersect_AdvanceFloorMissCache();
    assert(!QUERY() && planeTests > originalTests);

    /* Same addresses and frame, different plane. A cached miss is now a hit. */
    rdVector3 raised[] = {{0, 0, .04f}, {1, 0, .04f}, {0, 1, .04f}};
    rdMesh replacement = mesh;
    replacement.aVertices = raised;
    assert(rdModel3_ReplaceMesh(&model, 0, 0, &replacement));
    assert(QUERY() && hit == &triangle && distance > 0 && distance < .073f);
    start.x = 2;
    hit = &sentinel;
    normal = initial;
    for (int i = 0; i < 2; i++)
    {
        distance = 42;
        originalTests = planeTests;
        assert(!QUERY() && planeTests > originalTests && distance < .073f);
        assert(hit == &sentinel && !memcmp(&normal, &initial, sizeof normal));
    }
    FloorMissKey key;
    assert(sithIntersect_MakeFloorKey(&key, &start, &down, .073f, 0, flags));
    assert(!sithIntersect_MakeFloorKey(&key, &start, &down, NAN, 0, flags));
    sithIntersect_CloseFloorMissCache();
    assert(sithIntersect_paFloorMissSets == NULL);
    puts("Floor cache eligibility, exact keys, output writes, epoch and mesh replacement passed.");
    return 0;
}
