#ifndef MESHER_H
#define MESHER_H

#ifndef BM_VECTOR
#include <vector>
#define BM_VECTOR std::vector
#endif

#include <stdint.h>

// CS = chunk size (max 62)
static constexpr int CS = 62;

// Padded chunk size
static constexpr int CS_P = CS + 2;
static constexpr int CS_2 = CS * CS;
static constexpr int CS_P2 = CS_P * CS_P;
static constexpr int CS_P3 = CS_P * CS_P * CS_P;

struct MeshData {
    uint64_t* faceMasks = nullptr; // CS_2 * 6
    uint64_t* opaqueMask = nullptr; //CS_P2
    uint8_t* forwardMerged = nullptr; // CS_2
    uint8_t* rightMerged = nullptr; // CS
    BM_VECTOR<uint64_t>* vertices = nullptr;
    int vertexCount = 0;
    int maxVertices = 0;
    int faceVertexBegin[6] = { 0 };
    int faceVertexLength[6] = { 0 };
	

};

// @param[in] voxels: The input data includes duplicate edge data from neighboring chunks which is used
// for visibility culling. For optimal performance, your world data should already be structured
// this way so that you can feed the data straight into this algorithm.
// Input data is ordered in ZXY and is 64^3 which results in a 62^3 mesh.
//
// @param[out] meshData The allocated vertices in MeshData with a length of meshData.vertexCount.
void mesh(const uint8_t* voxels, MeshData& meshData);

#endif // MESHER_H
