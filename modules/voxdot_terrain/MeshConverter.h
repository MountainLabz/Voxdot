// GodotVoxelMesher.h
#pragma once
#ifndef GODOT_VOXEL_MESHER_H
#define GODOT_VOXEL_MESHER_H

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "mesher.h"
#include <chrono> // Required for std::chrono
#include <cstdint> // For uint8_t, uint64_t
#include <map> // For std::map to store material colors
#include <vector>

// Struct to hold mesh data in a format suitable for Godot's ArrayMesh
struct GodotMeshData {
	std::vector<float> vertices; // x, y, z floats
	std::vector<float> normals; // nx, ny, nz floats
	std::vector<float> uvs; // u, v floats
	std::vector<float> colors; // r, g, b, a floats (per-vertex color)
	std::vector<uint32_t> indices; // For indexed drawing (triangles)
};

// A mesher designed to produce data for Godot's ArrayMesh quickly
class GodotVoxelMesher {
public:
	GodotVoxelMesher(); // Constructor now initializes material colors
	~GodotVoxelMesher() = default;

	// Converts the uint64_t-packed quad data from your existing mesher
	// into Godot-compatible vertex, normal, UV, color, and index arrays.
	// The generated mesh will be relative to the origin (0,0,0) of the voxel chunk itself.
	//
	// @param meshData: The output MeshData struct from your `mesh` function,
	//                  containing the packed uint64_t quads.
	// @param voxelScale: The world unit size of a single voxel (e.g., 0.1f).
	// @return A GodotMeshData struct populated with the converted mesh data.
	void convertQuadsToGodotMesh(GodotMeshData &outGodotMeshData,
			const MeshData &meshData,
			float voxelScale) const;

	// Set a color for a specific material type using Godot::Vector4
	void setMaterialColor(uint8_t materialType, const Vector4 &color);

	// Get the color for a specific material type as Godot::Vector4
	Vector4 getMaterialColor(uint8_t materialType) const;

private:
	// Helper function to unpack a single uint64_t quad and add its vertices
	// to the GodotMeshData.
	void addQuadToGodotMesh(
			GodotMeshData &godotMeshData,
			uint64_t packedQuad,
			float voxelScale) const;

	// Map to store colors for different material types (uint8_t materialType -> Godot::Vector4 RGBA)
	std::map<uint8_t, Vector4> materialColors;
};

#endif // GODOT_VOXEL_MESHER_H
