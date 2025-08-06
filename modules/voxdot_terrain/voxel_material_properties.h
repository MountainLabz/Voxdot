#pragma once
#ifndef VOXEL_MATERIAL_PROPERTIES_H
#define VOXEL_MATERIAL_PROPERTIES_H

#include "core/io/resource.h" // Inherit from Resource
#include "core/object/class_db.h" // For GDCLASS

class VoxelMaterialProperties : public Resource {
	GDCLASS(VoxelMaterialProperties, Resource);

private:
	float metallicness;
	float emissiveness;
	// Add other properties here if needed (e.g., roughness, albedo_color)

protected:
	static void _bind_methods(); // Method to bind properties to Godot

public:
	VoxelMaterialProperties(); // Constructor

	void set_metallicness(float p_metallicness);
	float get_metallicness() const;

	void set_emissiveness(float p_emissiveness);
	float get_emissiveness() const;
};

#endif // VOXEL_MATERIAL_PROPERTIES_H
