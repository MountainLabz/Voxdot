// note: Next up is to remove GodotMeshData stuff make it all direct.
// first order of buissness: redo the collision

// GodotVoxelMesher.cpp
#include "MeshConverter.h"
#include "core/os/os.h" // For Godot's OS::get_singleton()->print
#include <array> // For std::array
#include <iostream> // For std::cout (though OS::get_singleton()->print is preferred for Godot)
#include <sstream> // For std::ostringstream for detailed logging

// Constants for unpacking the uint64_t quad.
// THESE MUST EXACTLY MATCH THE PACKING LOGIC IN YOUR `getQuad` function
// AND THE UNPACKING IN YOUR GLSL SHADER!
// Original packing from `getQuad` (as described in GLSL shader):
// x (bits 0-5), y (bits 6-11), z (bits 12-17), w (bits 18-23), h (bits 24-29), type (bits 32-39), normal_id (bits 40-42)

constexpr uint64_t X_MASK = 0x3Fu; // 6 bits for x
constexpr uint64_t Y_MASK = 0x3Fu; // 6 bits for y
constexpr uint64_t Z_MASK = 0x3Fu; // 6 bits for z
constexpr uint64_t W_MASK = 0x3Fu; // 6 bits for width
constexpr uint64_t H_MASK = 0x3Fu; // 6 bits for height
constexpr uint64_t TYPE_MASK = 0xFFu; // 8 bits for type
constexpr uint64_t NORMAL_ID_MASK = 0x7u; // 3 bits for normal_id

// Shift amounts for unpacking - derived directly from GLSL shader
constexpr int X_SHIFT = 0;
constexpr int Y_SHIFT = 6;
constexpr int Z_SHIFT = 12;
constexpr int W_SHIFT = 18;
constexpr int H_SHIFT = 24;
constexpr int TYPE_SHIFT = 32;
constexpr int NORMAL_ID_SHIFT = 40;

// Lookup tables for normals and face flipping, mirroring the GLSL shader
const std::array<Vector3, 6> NORMAL_LOOKUP = {
	Vector3(0, 1, 0), // 0: TOP (+Y)
	Vector3(0, -1, 0), // 1: BOTTOM (-Y)
	Vector3(1, 0, 0), // 2: RIGHT (+X)
	Vector3(-1, 0, 0), // 3: LEFT (-X)
	Vector3(0, 0, 1), // 4: FRONT (+Z)
	Vector3(0, 0, -1) // 5: BACK (-Z)
};

// This lookup determines which component of finalVertexPos is modified by 'w' and 'h'
// wDir = (normal_id & 2u) >> 1;
// hDir = 2u - (normal_id >> 2u);
// From GLSL: 0=X, 1=Y, 2=Z for components
// TOP (+Y, id 0): wDir=0 (X), hDir=2 (Z)
// BOTTOM (-Y, id 1): wDir=0 (X), hDir=2 (Z)
// RIGHT (+X, id 2): wDir=1 (Y), hDir=2 (Z)
// LEFT (-X, id 3): wDir=1 (Y), hDir=2 (Z)
// FRONT (+Z, id 4): wDir=0 (X), hDir=1 (Y)
// BACK (-Z, id 5): wDir=0 (X), hDir=1 (Y)
const std::array<int, 6> W_DIR_LOOKUP = { 0, 0, 1, 1, 0, 0 }; // 0:X, 1:Y, 2:Z
const std::array<int, 6> H_DIR_LOOKUP = { 2, 2, 2, 2, 1, 1 }; // 0:X, 1:Y, 2:Z

// Mirrors GLSL's flipLookup, applied to 'wMod' dimension for certain faces
const std::array<int, 6> FLIP_LOOKUP = { 1, -1, -1, 1, -1, 1 };

GodotVoxelMesher::GodotVoxelMesher() {
	// Initialize default material colors. These can be overridden later.
	// Example colors:
	materialColors[0] = Vector4(0.7f, 0.7f, 0.7f, 1.0f); // Default/placeholder
	materialColors[1] = Vector4(0.3f, 0.6f, 0.2f, 1.0f); // Grass green
	materialColors[2] = Vector4(0.5f, 0.3f, 0.1f, 1.0f); // Dirt brown (your terrainMaterialType)
	materialColors[3] = Vector4(0.6f, 0.6f, 0.6f, 1.0f); // Stone grey
	materialColors[4] = Vector4(0.8f, 0.8f, 0.2f, 1.0f); // Sand yellow
	materialColors[5] = Vector4(0.5f, 0.5f, 0.5f, 0.5f); // Sand yellow
}

void GodotVoxelMesher::setMaterialColor(uint8_t materialType, const Vector4 &color) {
	materialColors[materialType] = color;
}

Vector4 GodotVoxelMesher::getMaterialColor(uint8_t materialType) const {
	auto it = materialColors.find(materialType);
	if (it != materialColors.end()) {
		return it->second;
	}
	return Vector4(1.0f, 0.0f, 1.0f, 1.0f); // Return magenta for unassigned types
}

// Helper function to add a single quad's vertices and indices to GodotMeshData
void GodotVoxelMesher::addQuadToGodotMesh(
		GodotMeshData &godotMeshData,
		uint64_t packedQuad,
		float voxelScale) const {
	// Unpack quad data, mirroring GLSL shader exactly
	int x = static_cast<int>((packedQuad >> X_SHIFT) & X_MASK);
	int y = static_cast<int>((packedQuad >> Y_SHIFT) & Y_MASK);
	int z = static_cast<int>((packedQuad >> Z_SHIFT) & Z_MASK);
	int w = static_cast<int>((packedQuad >> W_SHIFT) & W_MASK); // Width of quad along its plane
	int h = static_cast<int>((packedQuad >> H_SHIFT) & H_MASK); // Height of quad along its plane
	uint8_t type = static_cast<uint8_t>((packedQuad >> TYPE_SHIFT) & TYPE_MASK);
	uint8_t normal_id = static_cast<uint8_t>((packedQuad >> NORMAL_ID_SHIFT) & NORMAL_ID_MASK);

	/*std::ostringstream oss_unpacked;
	oss_unpacked << "Unpacked quad: x=" << x << ", y=" << y << ", z=" << z
				 << ", w=" << w << ", h=" << h << ", type=" << static_cast<int>(type)
				 << ", normal_id=" << static_cast<int>(normal_id);
	OS::get_singleton()->print(oss_unpacked.str().c_str());*/

	if (normal_id >= NORMAL_LOOKUP.size()) {
		OS::get_singleton()->printerr("Error: Invalid normal_id unpacked");
		return;
	}

	Vector3 normal = NORMAL_LOOKUP[normal_id];
	Vector4 color = getMaterialColor(type);

	// Get axis directions for quad (matching GLSL's wDir and hDir)
	int w_dir_axis = W_DIR_LOOKUP[normal_id]; // 0=X, 1=Y, 2=Z
	int h_dir_axis = H_DIR_LOOKUP[normal_id]; // 0=X, 1=Y, 2=Z
	int flip_factor_w = FLIP_LOOKUP[normal_id]; // 1 or -1

	// Base position for the quad's origin corner (lowest x,y,z of the quad's plane)
	Vector3 base_pos = Vector3(x, y, z) * voxelScale;

	// The four corners of the quad (before normal application for 3D position)
	// These are relative offsets based on w and h along w_dir_axis and h_dir_axis
	std::array<Vector3, 4> quad_corners_relative;
	quad_corners_relative[0] = Vector3(0, 0, 0); // Corner 0
	quad_corners_relative[1] = Vector3(0, 0, 0); // Corner 1
	quad_corners_relative[2] = Vector3(0, 0, 0); // Corner 2
	quad_corners_relative[3] = Vector3(0, 0, 0); // Corner 3

	// Apply width and height extensions along the determined axes.
	// The GLSL shader:
	// finalVertexPos[wDir] += (float(w) * u_voxelScale) * float(wMod) * float(flipLookup[normal_id]);
	// finalVertexPos[hDir] += (float(h) * u_voxelScale) * float(hMod);
	//
	// wMod = corner >> 1 (0 for corners 0,1; 1 for corners 2,3)
	// hMod = corner & 1 (0 for corners 0,2; 1 for corners 1,3)

	// Corner 0: (x,y,z) -- no offset
	// quad_corners_relative[0] remains Vector3(0,0,0)

	// Corner 1: +h (relative)
	quad_corners_relative[1][h_dir_axis] += static_cast<float>(h) * voxelScale;

	// Corner 2: +w (relative)
	quad_corners_relative[2][w_dir_axis] += static_cast<float>(w) * voxelScale * flip_factor_w;

	// Corner 3: +w, +h (relative)
	quad_corners_relative[3][w_dir_axis] += static_cast<float>(w) * voxelScale * flip_factor_w;
	quad_corners_relative[3][h_dir_axis] += static_cast<float>(h) * voxelScale;

	// Indices for two triangles forming the quad (matching GLSL vertexToCornerMap logic)
	// GLSL vertexToCornerMap: 0,1,2, 1,3,2
	// Which means if we have vertices indexed 0,1,2,3 for the quad corners:
	// Triangle 1: V0, V1, V2
	// Triangle 2: V1, V3, V2
	const std::array<int, 6> indices_local = { 0, 2, 1, 1, 2, 3 };

	// Add vertices, normals, UVs, and colors
	// We add 4 vertices for each quad
	uint32_t current_vertex_base_index = godotMeshData.vertices.size() / 3; // Number of existing vertices

	for (int i = 0; i < 4; ++i) {
		Vector3 final_vertex_pos = base_pos + quad_corners_relative[i];

		// Add position
		godotMeshData.vertices.push_back(final_vertex_pos.x);
		godotMeshData.vertices.push_back(final_vertex_pos.y);
		godotMeshData.vertices.push_back(final_vertex_pos.z);

		// Add normal
		godotMeshData.normals.push_back(normal.x);
		godotMeshData.normals.push_back(normal.y);
		godotMeshData.normals.push_back(normal.z);

		// Add UVs (simple quad UVs for corners 0,1,2,3)
		// Corresponds to corners as they are calculated:
		// 0 -> (0,0)
		// 1 -> (0,1)  (along h-axis)
		// 2 -> (1,0)  (along w-axis)
		// 3 -> (1,1)  (along w-axis and h-axis)
		if (i == 0) {
			godotMeshData.uvs.push_back(0.0f);
			godotMeshData.uvs.push_back(0.0f);
		} else if (i == 1) {
			godotMeshData.uvs.push_back(0.0f);
			godotMeshData.uvs.push_back(1.0f);
		} else if (i == 2) {
			godotMeshData.uvs.push_back(1.0f);
			godotMeshData.uvs.push_back(0.0f);
		} else if (i == 3) {
			godotMeshData.uvs.push_back(1.0f);
			godotMeshData.uvs.push_back(1.0f);
		}

		// Add color
		godotMeshData.colors.push_back(color.x);
		godotMeshData.colors.push_back(color.y);
		godotMeshData.colors.push_back(color.z);
		godotMeshData.colors.push_back(color.w);

		/*std::ostringstream oss_vertex_pos;
		oss_vertex_pos << "  Vertex " << i << " (Global ID " << current_vertex_base_index + i
					   << "): Pos=(" << final_vertex_pos.x << "," << final_vertex_pos.y << "," << final_vertex_pos.z
					   << "), Normal=(" << normal.x << "," << normal.y << "," << normal.z << ")";
		OS::get_singleton()->print(oss_vertex_pos.str().c_str());*/
	}

	// Add indices for the two triangles

	for (int i = 0; i < 6; ++i) {
		godotMeshData.indices.push_back(current_vertex_base_index + indices_local[i]);
	}
}

void GodotVoxelMesher::convertQuadsToGodotMesh( GodotMeshData &outGodotMeshData,
		const MeshData &meshData,
		float voxelScale) const {
	//GodotMeshData godotMeshData;

	// Pre-allocate vectors for performance
	// Each quad produces 4 vertices, each with 3 position, 3 normal, 2 UV, and 4 color components
	// And 6 indices for 2 triangles
	outGodotMeshData.vertices.reserve(meshData.vertexCount * 4 * 3);
	outGodotMeshData.normals.reserve(meshData.vertexCount * 4 * 3);
	outGodotMeshData.uvs.reserve(meshData.vertexCount * 4 * 2);
	outGodotMeshData.colors.reserve(meshData.vertexCount * 4 * 4);
	outGodotMeshData.indices.reserve(meshData.vertexCount * 6);

	/*std::ostringstream oss_conversion_start;
	oss_conversion_start << "Starting quad conversion. Input vertexCount (quads): " << meshData.vertexCount;
	OS::get_singleton()->print(oss_conversion_start.str().c_str());*/

	// Iterate through each packed quad and add it to the Godot mesh data
	if (meshData.vertices && meshData.vertexCount > 0) {
		for (int i = 0; i < meshData.vertexCount; ++i) {
			addQuadToGodotMesh(outGodotMeshData, (*meshData.vertices)[i], voxelScale);
		}
	} else {
		OS::get_singleton()->printerr("convertQuadsToGodotMesh: meshData.vertices is null or vertexCount is 0. No quads to convert.");
	}

	std::ostringstream oss_conversion_end;
	/*oss_conversion_end << "Converted " << meshData.vertexCount << " quads to Godot mesh: "
					   << godotMeshData.vertices.size() / 3 << " vertices, "
					   << godotMeshData.indices.size() / 3 << " triangles.";
	OS::get_singleton()->print(oss_conversion_end.str().c_str());*/

//	return godotMeshData;
}
