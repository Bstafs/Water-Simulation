#include "FluidMaths.hlsl"

struct ParticleAttributes
{
    float3 position; // 12 bytes
    float nearDensity; // 4 bytes (16-byte aligned)

    float3 velocity; // 12 bytes
    float density; // 4 bytes (16-byte aligned)
};

cbuffer SimulationParams : register(b0)
{
    int numParticles; // Total number of particles
    float deltaTime;
    float minX;
    float minZ;
};

cbuffer BitonicParams : register(b1)
{
    uint numElements;
    uint k;
    uint j;
    uint padding;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float2 texCoord;
};

static const uint edgeTable[256] =
{
    0x0, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66, 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff, 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55, 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66, 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa, 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33, 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99, 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

static const int triTable[256][16] =
{
    { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1 },
    { 8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1 },
    { 3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1 },
    { 4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1 },
    { 4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1 },
    { 9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1 },
    { 10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1 },
    { 5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1 },
    { 5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1 },
    { 8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1 },
    { 2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1 },
    { 2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1 },
    { 11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1 },
    { 5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1 },
    { 11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1 },
    { 11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1 },
    { 2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1 },
    { 6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1 },
    { 3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1 },
    { 6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1 },
    { 6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1 },
    { 8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1 },
    { 7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1 },
    { 3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1 },
    { 0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1 },
    { 9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1 },
    { 8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1 },
    { 5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1 },
    { 0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1 },
    { 6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1 },
    { 10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1 },
    { 1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1 },
    { 0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1 },
    { 3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1 },
    { 6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1 },
    { 9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1 },
    { 8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1 },
    { 3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1 },
    { 6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1 },
    { 10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1 },
    { 10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1 },
    { 2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1 },
    { 7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1 },
    { 7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1 },
    { 2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1 },
    { 1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1 },
    { 11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1 },
    { 8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1 },
    { 0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1 },
    { 7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1 },
    { 7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1 },
    { 10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1 },
    { 0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1 },
    { 7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1 },
    { 6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1 },
    { 6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1 },
    { 4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1 },
    { 10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1 },
    { 8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1 },
    { 1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1 },
    { 10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1 },
    { 10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1 },
    { 9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1 },
    { 7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1 },
    { 3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1 },
    { 7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1 },
    { 3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1 },
    { 6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1 },
    { 9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1 },
    { 1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1 },
    { 4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1 },
    { 7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1 },
    { 6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1 },
    { 0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1 },
    { 6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1 },
    { 0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1 },
    { 11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1 },
    { 6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1 },
    { 5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1 },
    { 9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1 },
    { 1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1 },
    { 10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1 },
    { 0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1 },
    { 10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1 },
    { 11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1 },
    { 9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1 },
    { 7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1 },
    { 2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1 },
    { 9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1 },
    { 9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1 },
    { 1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1 },
    { 5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1 },
    { 0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1 },
    { 10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1 },
    { 2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1 },
    { 0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1 },
    { 0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1 },
    { 9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1 },
    { 5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1 },
    { 5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1 },
    { 8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1 },
    { 9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1 },
    { 1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1 },
    { 3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1 },
    { 4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1 },
    { 9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1 },
    { 11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1 },
    { 11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1 },
    { 2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1 },
    { 9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1 },
    { 3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1 },
    { 1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1 },
    { 4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1 },
    { 0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1 },
    { 1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
};

// Particles Info
RWStructuredBuffer<ParticleAttributes> Partricles : register(u0); // Output UAV

// Spatial Grid & Bitonic Sort
RWStructuredBuffer<uint3> GridIndices : register(u1); // Grid buffer: holds indices to particles
RWStructuredBuffer<uint> GridOffsets : register(u2); // Tracks number of particles per cell

// Radix Sort - Not Being Used Currently
RWStructuredBuffer<uint> Histogram : register(u3);
RWStructuredBuffer<uint> PrefixSum : register(u4);
RWStructuredBuffer<uint> GlobalParams : register(u5);
RWStructuredBuffer<uint3> SortedGridIndices : register(u6);

// Marching Cubes
RWStructuredBuffer<Vertex> VertexBuffer : register(u7);
RWStructuredBuffer<uint> IndexBuffer : register(u8);
RWStructuredBuffer<uint> VertexCount : register(u9);
RWStructuredBuffer<uint> IndexCount : register(u10);

static const float targetDensity = 10.0f;
static const float stiffnessValue = 100.0f;
static const float nearStiffnessValue = 400.0f;
static const float smoothingRadius = 2.5f;
static const float sqrRadius = smoothingRadius * smoothingRadius;
static const uint particlesPerCell = 100;
static const int ThreadCount = 256;

static const uint hashK1 = 15823;
static const uint hashK2 = 9737333;
static const uint hashK3 = 440817757;

static const uint RADIX_BITS = 8; // 8-bit digits
static const uint RADIX = (1 << RADIX_BITS); // 256 buckets
static const uint MAX_PARTICLES = 1 << 20; // Adjust based on your needs

static const int3 offsets3D[27] =
{
    int3(-1, -1, -1),
	int3(-1, -1, 0),
	int3(-1, -1, 1),
	int3(-1, 0, -1),
	int3(-1, 0, 0),
	int3(-1, 0, 1),
	int3(-1, 1, -1),
	int3(-1, 1, 0),
	int3(-1, 1, 1),
	int3(0, -1, -1),
	int3(0, -1, 0),
	int3(0, -1, 1),
	int3(0, 0, -1),
	int3(0, 0, 0),
	int3(0, 0, 1),
	int3(0, 1, -1),
	int3(0, 1, 0),
	int3(0, 1, 1),
	int3(1, -1, -1),
	int3(1, -1, 0),
	int3(1, -1, 1),
	int3(1, 0, -1),
	int3(1, 0, 0),
	int3(1, 0, 1),
	int3(1, 1, -1),
	int3(1, 1, 0),
	int3(1, 1, 1)
};

// Utility functions
int3 GetCell3D(float3 position, float radius)
{
    return (int3) floor(position / radius);
}

uint HashCell3D(int3 cell)
{
    cell = (uint3) cell;
    return (cell.x * hashK1) + (cell.y * hashK2) + (cell.z * hashK3);
}

uint KeyFromHash(uint hash, uint tableSize)
{
    return hash % tableSize;
}

uint GetBits(uint value, uint bitOffset, uint numBits)
{
    return (value >> bitOffset) & ((1 << numBits) - 1);
}


[numthreads(ThreadCount, 1, 1)]
void ClearGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    GridOffsets[dispatchThreadId.x] = 0;
    GridIndices[dispatchThreadId.x] = uint3(0, 0, 0);
    SortedGridIndices[dispatchThreadId.x] = uint3(0, 0, 0);
}

[numthreads(ThreadCount, 1, 1)]
void AddParticlesToGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    uint index = dispatchThreadId.x;
    int3 cell = GetCell3D(Partricles[index].position, smoothingRadius);
    uint hash = HashCell3D(cell);
    uint key = KeyFromHash(hash, numParticles);
    
        // Populate the grid buffer with particle information
    GridIndices[dispatchThreadId.x] = uint3(
            index, // Store the particle index
            hash, // Store the hash
            key // Reserved for key
        );
}

// Bitonic sort compute shader
[numthreads(ThreadCount, 1, 1)]
void BitonicSort(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    if (i >= numElements)
        return;

    // Compute the partner index
    uint ixj = i ^ j;

    // Only process if i < ixj 
    if (ixj > i && ixj < numElements)
    {
        // Decide the sort direction for this bitonic block
        bool ascending = ((i & k) == 0);

        // Load the two elements. 
        uint3 elemI = GridIndices[i];
        uint3 elemJ = GridIndices[ixj];

        // Compare keys 
        if ((ascending && (elemI.z > elemJ.z)) ||
    (!ascending && (elemI.z < elemJ.z)))
        {
            // Swap the two elements
            GridIndices[i] = elemJ;
            GridIndices[ixj] = elemI;
        }
    }
}

// 1. Histogram Pass
[numthreads(ThreadCount, 1, 1)]
void RadixHistogram(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{
    // Clear local histogram
    if (groupThreadID.x < RADIX)
    {
        Histogram[dispatchThreadID.y * RADIX + groupThreadID.x] = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    uint particleIdx = dispatchThreadID.x;
    if (particleIdx >= numParticles)
        return;
    
    uint currentBit = GlobalParams[0];
    uint hash = GridIndices[particleIdx].y;
    uint digit = GetBits(hash, currentBit, RADIX_BITS);
    
    InterlockedAdd(Histogram[dispatchThreadID.y * RADIX + digit], 1);
}

// 2. Prefix SumPass
[numthreads(RADIX, 1, 1)]
void RadixPrefixSum(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint digit = dispatchThreadID.x;
    uint sum = 0;
    
    // Sum histograms from all groups
    for (uint group = 0; group < (numParticles + ThreadCount - 1) / ThreadCount; group++)
    {
        sum += Histogram[group * RADIX + digit];
    }
    
    PrefixSum[digit] = sum;
    
    // Initialize global offset if first digit
    if (digit == 0)
    {
        GlobalParams[1] = 0;
    }
}

// 3. Prepare Offsets Pass
[numthreads(1, 1, 1)]
void RadixPrepareOffsets()
{
    // Exclusive prefix sum
    uint sum = 0;
    for (uint i = 0; i < RADIX; i++)
    {
        uint temp = PrefixSum[i];
        PrefixSum[i] = sum;
        sum += temp;
    }
    
    // Update bit offset for next pass
    GlobalParams[0] += RADIX_BITS;
    GlobalParams[2]++; // Increment pass count
}
// 3. Scatter Pass

// Shared memory for prefix sums
groupshared uint sharedPrefix[RADIX];

[numthreads(ThreadCount, 1, 1)]
void RadixScatter(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{
    // Load prefix sums into shared memory
    if (groupThreadID.x < RADIX)
    {
        sharedPrefix[groupThreadID.x] = PrefixSum[groupThreadID.x];
    }
    GroupMemoryBarrierWithGroupSync();
    
    uint particleIdx = dispatchThreadID.x;
    if (particleIdx >= numParticles)
        return;
    
    uint currentBit = GlobalParams[0] - RADIX_BITS; // Previous bit offset
    uint hash = GridIndices[particleIdx].y;
    uint digit = GetBits(hash, currentBit, RADIX_BITS);
    
    uint newIndex;
    InterlockedAdd(sharedPrefix[digit], 1, newIndex);
    
    // Create a temporary buffer or ping-pong between two buffers
    SortedGridIndices[newIndex] = GridIndices[particleIdx];
}

[numthreads(ThreadCount, 1, 1)]
void BuildGridOffsets(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    if (i >= numElements)
        return;

    uint currentHash = GridIndices[i].y;
    uint previousHash = (i == 0) ? 0xFFFFFFFF : GridIndices[i - 1].y;

    if (i == 0 || currentHash != previousHash)
    {
        uint cellKey = KeyFromHash(currentHash, numParticles);
        GridOffsets[cellKey] = i;
    }
}

[numthreads(ThreadCount, 1, 1)]
void CalculateDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    
    float3 position = Partricles[dispatchThreadId.x].position;
    float density = 0.0f;
    float nearDensity = 0.0f;
    float mass = 1.0f;

    int3 gridIndex = GetCell3D(position, smoothingRadius);
    
    for (int i = 0; i < 27; i++)
    {
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        int currentIndex = GridOffsets[key];
         
        if (currentIndex >= numParticles)
            continue;
                
        while (currentIndex < numParticles)
        {
            uint3 indexData = GridIndices[currentIndex];
            currentIndex++;

			// Skip if hash does not match
            if (indexData[2] != key)
                break;
            
            if (indexData[1] != hash)
                continue;
                                 
            uint neighborParticleIndex = indexData[0];
            
            float3 offset = Partricles[neighborParticleIndex].position - position;
            float sqrDst = dot(offset, offset);
                     
            if (sqrDst > sqrRadius)
                continue;
                          
            float dst = sqrt(sqrDst);
            density += mass * DensitySmoothingKernel(dst, smoothingRadius);
            nearDensity += mass * NearDensitySmoothingKernel(dst, smoothingRadius);
        }                 
    }

    Partricles[dispatchThreadId.x].density = density;
    Partricles[dispatchThreadId.x].nearDensity = nearDensity;
}

float ConvertDensityToPressure(float density)
{
    float densityError = density - targetDensity;
    return max(densityError, 0.0f) * stiffnessValue;
}

float ConvertNearDensityToPressure(float nearDensity)
{
    return max(nearDensity, 0.0f) * nearStiffnessValue;
}

float CalculateSharedPressure(float pressureA, float pressureB)
{
    return (pressureA + pressureB) / 2.0f;
}

[numthreads(ThreadCount, 1, 1)]
void CalculatePressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    float3 position = Partricles[dispatchThreadId.x].position;
    float3 velocity = Partricles[dispatchThreadId.x].velocity;
    float density = Partricles[dispatchThreadId.x].density;
    float nearDensity = Partricles[dispatchThreadId.x].nearDensity;
    
    // Pressure values for the current particle
    float pressure = ConvertDensityToPressure(density);
    float nearPressure = ConvertNearDensityToPressure(nearDensity);

    float3 pressureForce = float3(0.0f, 0.0f, 0.0f);
    float3 repulsionForce = float3(0.0f, 0.0f, 0.0f);
    float3 viscousForce = float3(0.0f, 0.0f, 0.0f);
    int3 gridIndex = GetCell3D(position, smoothingRadius);
    float viscosityCoefficient = 0.01f;
    
    for (int i = 0; i < 27; i++)
    {
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = GridOffsets[key];
     
        if (currIndex >= numParticles)
            continue;
                
        while (currIndex < numParticles)
        {
            uint3 indexData = GridIndices[currIndex];
            currIndex++;
            
			// Skip if hash does not match
            if (indexData[2] != key)
                break;
            
            if (indexData[1] != hash)
                continue;
            
            uint neighbourIndex = indexData[0];
            if (neighbourIndex == dispatchThreadId.x)
                continue;

            
             // Get the neighbor's position, velocity, and other properties
            float3 neighborPosition = Partricles[neighbourIndex].position;
            float3 neighborVelocity = Partricles[neighbourIndex].velocity;
         
            float3 relativeVelocity = neighborVelocity - velocity;
            
            float3 offset = neighborPosition - position;
            float sqrDst = dot(offset, offset);
                                      
            if (sqrDst > sqrRadius)
                continue;
            
            float neighborDensity = Partricles[neighbourIndex].density;
            float neighborNearDensity = Partricles[neighbourIndex].nearDensity;
            float neighbourPressure = ConvertDensityToPressure(neighborDensity);
            float neighbourPressureNear = ConvertNearDensityToPressure(neighborNearDensity);
                             
            float sharedPressure = CalculateSharedPressure(pressure, neighbourPressure);
            float sharedNearPressure = CalculateSharedPressure(nearPressure, neighbourPressureNear);
            
            float dst = sqrt(sqrDst);
                    
          // Stops particles getting stuck inside each other and causing velocity to go NaN.
            float3 dir = dst > 0 ? offset / dst : float3(0, 1, 0);
                  
            float poly6 = ViscositySmoothingKernel(dst, smoothingRadius);
            float kernelDerivative = PressureSmoothingKernel(dst, smoothingRadius);
            float nearKernelDerivative = NearDensitySmoothingKernelDerivative(dst, smoothingRadius);
                    
            pressureForce += dir * kernelDerivative * sharedPressure / neighborDensity;
            repulsionForce += dir * nearKernelDerivative * sharedNearPressure / neighborNearDensity;
            viscousForce += relativeVelocity * viscosityCoefficient * poly6;
        }          
    }

    // Sum up the forces from pressure and repulsion
    float3 totalForce = pressureForce + repulsionForce + viscousForce;
    
    // Apply the calculated force to update the particle's velocity (acceleration = force / density)
    float invDensity = density > 0.0001f ? 1.0f / density : 0.0f;
    float3 acceleration = totalForce * invDensity;
    
    Partricles[dispatchThreadId.x].velocity.xyz += acceleration * deltaTime;
}

void CollisionBox(inout float3 pos, inout float3 velocity, float minX, float maxX, float minZ, float maxZ)
{
    float minY = -30.0f;

    float maxY = 50.0f;
    
    float dampingFactor = 0.99f;
    
    if (pos.x < minX)
    {
        pos.x = minX;
        velocity.x *= -1.0f;
    }
    else if (pos.x > maxX)
    {
        pos.x = maxX;
        velocity.x *= -1.0f;
    }

    if (pos.y < minY)
    {
        pos.y = minY;
        velocity.y *= -1.0f;
    }
    else if (pos.y > maxY)
    {
        pos.y = maxY;
        velocity.y *= -1.0f;
    }

    if (pos.z < minZ)
    {
        pos.z = minZ;
        velocity.z *= -1.0f;
    }
    else if (pos.z > maxZ)
    {
        pos.z = maxZ;
        velocity.z *= -1.0f;
    }

	// Apply damping to velocity after collision
    velocity *= dampingFactor;
}

[numthreads(ThreadCount, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    if (dispatchThreadID.x >= numParticles)
        return;
             
    float3 inputPosition = Partricles[dispatchThreadID.x].position;
    float3 inputVelocity = Partricles[dispatchThreadID.x].velocity;
    float3 inputDensity = Partricles[dispatchThreadID.x].density;
    
    inputVelocity.y += -9.807f * deltaTime;
         
    inputPosition += inputVelocity * deltaTime;
    
    CollisionBox(inputPosition, inputVelocity, minX, -minX, minZ, -minZ);
    
    Partricles[dispatchThreadID.x].velocity = inputVelocity;
    Partricles[dispatchThreadID.x].position = inputPosition;
    Partricles[dispatchThreadID.x].density = inputDensity;
}

static const float isolevel = 10.0f;
static const float3 gridOrigin = float3(0,0,0);
static const float cellSize = 1.0f;
static const uint3 gridResolution = float3(64,64,64);
static const uint maxVertices = (gridResolution.x * gridResolution.y * gridResolution.z * 5) * 3;


static const int2 edgeConnections[12] =
{
    int2(0, 1), int2(1, 2), int2(2, 3), int2(3, 0), // Bottom edges
    int2(4, 5), int2(5, 6), int2(6, 7), int2(7, 4), // Top edges
    int2(0, 4), int2(1, 5), int2(2, 6), int2(3, 7) // Vertical edges
};

float SampleDensityAtPoint(float3 worldPos)
{
    float density = 0.0f;
    float mass = 1.0f;
    
    int3 gridIndex = GetCell3D(worldPos, smoothingRadius);
    
    for (int i = 0; i < 27; i++)
    {
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        int currentIndex = GridOffsets[key];
         
        if (currentIndex >= numParticles)
            continue;
                
        while (currentIndex < numParticles)
        {
            uint3 indexData = GridIndices[currentIndex];
            currentIndex++;

            if (indexData[2] != key)
                break;
            
            if (indexData[1] != hash)
                continue;
                                 
            uint neighborParticleIndex = indexData[0];
            float3 offset = Partricles[neighborParticleIndex].position - worldPos;
            float sqrDst = dot(offset, offset);
                     
            if (sqrDst > sqrRadius)
                continue;
                          
            float dst = sqrt(sqrDst);
            density += mass * DensitySmoothingKernel(dst, smoothingRadius);
        }
    }
    return density;
}

[numthreads(8, 8, 8)]
void MarchingCubesCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (any(dispatchThreadID >= (gridResolution - uint3(1, 1, 1))))
        return;
    
    // Calculate the 8 corners of this grid cell
    float3 corners[8];
    float densities[8];
    
    for (int i = 0; i < 8; i++)
    {
        uint3 cornerIndex = dispatchThreadID + uint3((i & 1), ((i >> 1) & 1), ((i >> 2) & 1));
        corners[i] = gridOrigin + cornerIndex * cellSize;
        densities[i] = SampleDensityAtPoint(corners[i]);
    }
    
    // Determine cube index (same as before)
    int cubeIndex = 0;
    if (densities[0] < isolevel)
        cubeIndex |= 1;
    if (densities[1] < isolevel)
        cubeIndex |= 2;
    if (densities[2] < isolevel)
        cubeIndex |= 3;
    if (densities[3] < isolevel)
        cubeIndex |= 4;
    if (densities[4] < isolevel)
        cubeIndex |= 5;
    if (densities[5] < isolevel)
        cubeIndex |= 6;
    if (densities[6] < isolevel)
        cubeIndex |= 7;
    if (densities[7] < isolevel)
        cubeIndex |= 8;
    
    if (edgeTable[cubeIndex] == 0)
        return;
    
    // Find vertices where surface intersects the cube
    float3 vertices[12];
    for (int edge = 0; edge < 12; edge++)
    {
        if (edgeTable[cubeIndex] & (1 << edge))
        {
            int a = edgeConnections[edge].x;
            int b = edgeConnections[edge].y;
            
            float t = (isolevel - densities[a]) / (densities[b] - densities[a]);
            vertices[edge] = lerp(corners[a], corners[b], t);
        }
    }
    
    // Generate triangles (using your existing Triangle/Vertex buffers)
    for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
    {
        //uint baseIndex;
       // InterlockedAdd(VertexCount[0], 3, baseIndex);
        
        //if (baseIndex + 2 >= maxVertices)
        //    return;
            
        //// Add vertices
        //VertexBuffer[baseIndex].position = vertices[triTable[cubeIndex][i]];
        //VertexBuffer[baseIndex + 1].position = vertices[triTable[cubeIndex][i + 1]];
        //VertexBuffer[baseIndex + 2].position = vertices[triTable[cubeIndex][i + 2]];
        
        //// Calculate normal
        //float3 normal = normalize(cross(
        //    VertexBuffer[baseIndex + 1].position - VertexBuffer[baseIndex].position,
        //    VertexBuffer[baseIndex + 2].position - VertexBuffer[baseIndex].position
        //));
        
        //VertexBuffer[baseIndex].normal = normal;
        //VertexBuffer[baseIndex + 1].normal = normal;
        //VertexBuffer[baseIndex + 2].normal = normal;
        
       //// Calculate TexCoords
       // float3 minPos = gridOrigin;
       // float3 maxPos = gridOrigin + gridResolution * cellSize;
       // float3 range = maxPos - minPos;
        
       // VertexBuffer[baseIndex].texCoord = float2(
       //     (VertexBuffer[baseIndex].position.x - minPos.x) / range.x,
       //     (VertexBuffer[baseIndex].position.z - minPos.z) / range.z
       // );
        
       // VertexBuffer[baseIndex + 1].texCoord = float2(
       //     (VertexBuffer[baseIndex + 1].position.x - minPos.x) / range.x,
       //     (VertexBuffer[baseIndex + 1].position.z - minPos.z) / range.z
       // );
        
       // VertexBuffer[baseIndex + 2].texCoord = float2(
       //     (VertexBuffer[baseIndex + 2].position.x - minPos.x) / range.x,
       //     (VertexBuffer[baseIndex + 2].position.z - minPos.z) / range.z
       // );
        
        //// Add indices 
        //uint indexBase;
        //InterlockedAdd(IndexCount[0], 3, indexBase);
        //IndexBuffer[indexBase] = baseIndex;
        //IndexBuffer[indexBase + 1] = baseIndex + 1;
        //IndexBuffer[indexBase + 2] = baseIndex + 2;
    }
}