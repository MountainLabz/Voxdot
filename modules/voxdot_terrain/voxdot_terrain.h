#pragma once
#include <chrono> // Required for std::chrono
#include <iostream> // For output

//#include "thirdparty/misc/FastNoiseLite.h"
#include "MeshConverter.h" // For GodotMeshData
#include "core/math/transform_3d.h" // Required for Transform3D
#include "mesher.h"

#include "scene/3d/mesh_instance_3d.h" // Required for MeshInstance3D
#include "scene/3d/node_3d.h"
#include "scene/resources/mesh.h"
#include "terrain_chunk_types.h" // New: For ChunkMetadata, SDF types, HashMap types

#include "scene/3d/physics/static_body_3d.h" // Required for StaticBody3D
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include <modules/noise/fastnoise_lite.h>
#include "core/object/class_db.h"

#include "world_gen.h"


class VoxdotTerrain : public StaticBody3D {
	GDCLASS(VoxdotTerrain, StaticBody3D);

	// Chunk Management Members
	HashMap<Vector3, ChunkMetadata, Vector3Hasher, Vector3Equals> chunk_map;
	Ref<FastNoiseLite> noise; 
	float voxel_scale; // Scale of a single voxel in world units
	float noise_max;
	float noise_base;

	Vector<Vector3> dirty_chunks;
	Vector<MeshInstance3D *> mesh_instance_pool; // Pool for reusable MeshInstance3Ds
	MeshData m_reuseable_meshdata;

	Vector<Ref<Biome>> biomes;

	GodotVoxelMesher _voxel_mesher;


	Vector<CollisionShape3D *> collision_shape_pool_free;
	Vector<CollisionShape3D *> collision_shape_pool_in_use;

	Ref<Material> shared_material;

private:
	



protected:
	static void _bind_methods();

public:
	VoxdotTerrain();
	~VoxdotTerrain(); // Add destructor for proper cleanup

	void import_palette_png(const String &p_path);


	// Terrain system initialization/cleanup
	void init_terrain_system(float initial_voxel_scale = 0.1f, int noise_seed = 12345, int pool_size = 500);
	void cleanup_terrain_system();

	// Chunk lifecycle
	void add_chunk(const Vector3 &coords, bool empty);
	void remove_chunk(const Vector3 &coords);
	bool has_chunk(const Vector3 &coords) const;
	void clear_all_chunks();
	Vector<Vector3> get_loaded_chunk_coords() const;

	// SDF Edit application
	// chunk_size_in_voxels refers to the padded chunk size (e.g., 64)
	bool add_sdf_edit_at_world_pos(const Ref<ISDFEdit> &edit, int chunk_size_in_voxels);

	void add_edit_wrapper(Vector3 size, Vector3 world_pos, int material, int shape);

	void add_vox_edit_wrapper(String path, Vector3 world_pos, int material);

	// Mesh generation and child addition
	void create_and_add_mesh_instance_child(
			const GodotMeshData &godot_mesh_data,
			const Vector3 &chunk_coords_vector,
			float p_voxel_scale,
			bool p_add_collision_body = true); // Renamed p_voxel_scale to avoid conflict with member

	// Internal helper functions for voxel/mesh generation
	// These could potentially be static or in a separate utility class,
	// but are here for simplicity as per request.
	void populate_chunk_voxels(
			std::vector<uint8_t> &voxels,
			int cs_p_val,
			int cs_p2_val,
			int cs_p3_val,
			Vector3 chunk_offset_in_voxels,
			const Vector<Ref<ISDFEdit>> &sdf_edits,
			bool generate_terrain);
	void update_chunk_voxels(
			std::vector<uint8_t> &voxels,
			int cs_p_val, // Padded chunk size, e.g., 64
			int cs_p2_val, // Padded chunk size squared
			int cs_p3_val, // Padded chunk size cubed
			Vector3 chunk_offset_in_voxels // World-voxel offset for the chunk
	);


	void generate_mesh_data(MeshData &outMeshData, const std::vector<uint8_t> &voxels);
	void generate_godot_mesh_for_chunk(GodotMeshData &outGodotMeshData, const Vector3 &chunk_coords, int chunk_size_in_voxels);


	// Helper functions for the pool
	MeshInstance3D *get_mesh_instance_from_pool();
	void return_mesh_instance_to_pool(MeshInstance3D *mesh_instance);




	CollisionShape3D *get_collision_shape_from_pool(const Vector3 &p_chunk_coords);
	void return_collision_shape_to_pool(CollisionShape3D *p_collision_shape);
	void clear_collision_shape_pool();

	CollisionShape3D *create_and_add_collision_shape_child(const GodotMeshData &p_godot_mesh_data, const Vector3 &p_chunk_coords, float p_voxel_scale);
	
	bool is_chunk_partially_filled(const Vector3 &chunk_coords, float padding) const;

	void recreate_chunk_mesh(const Vector3 &chunk_coords, int chunk_size_in_voxels);
	void process_dirty_chunks(int max_chunks_to_process, bool process_oldest_chunks_first);

	// Accessors for voxel_scale (can be exposed to Godot Editor)
	void set_voxel_scale(float p_scale);
	float get_voxel_scale() const;

	// Accessors for shared_material
	void set_shared_material(Ref<Material> p_material);
	Ref<Material> get_shared_material() const;

	// Accessors for noise resource
	void set_noise(Ref<FastNoiseLite> p_noise);
	Ref<FastNoiseLite> get_noise() const;


	// Accessors for noise_max and noise_base
	void set_noise_max(float p_max);
	float get_noise_max() const;

	void set_noise_base(float p_base);
	float get_noise_base() const;

	void set_biomes(const Array &p_biomes);
	Array get_biomes() const;

};
