// must add a terrain generation flag, so that we can call a function that removes the other terrain meshes to be "generated" and fill it with the current ones
// to save time generating meshes that don't need to exist or get updated.

// fix the digging bug!

// add the terrain layers / biomes stuff

#include "voxdot_terrain.h"
#include "MeshConverter.h" // For GodotVoxelMesher and GodotMeshData
#include "core/math/math_funcs.h" // For Math::abs, Math::min, Math::max, floor, ceil
#include "core/os/os.h" // For Godot's printing (OS::get_singleton()->print_err, print)
#include "core/variant/array.h" // For Godot's Array type
#include "core/io/image.h" // Required for Image class
#include "core/io/resource_loader.h" // For loading resources
//#include "mesher.h"
#include "scene/resources/mesh.h" // For ArrayMesh
#include <sstream> // For std::ostringstream

#define OGT_VOX_IMPLEMENTATION
#include "ogt_vox.h"

// ----------------------------------------------------------------------------
// SDF Edit Binding and Implementation (for ISDFEdit, SDFSphereEdit, SDFCubeEdit)
// These _bind_methods are crucial for Godot to recognize and allow scripting
// with these custom types.
// ----------------------------------------------------------------------------

void ISDFEdit::_bind_methods() {
	// No direct properties to bind here as it's an interface, but crucial for Godot
	// to recognize this base class for inheritance and Ref<T> handling.
}

void SDFSphereEdit::_bind_methods() {
}

void SDFCubeEdit::_bind_methods() {
}

// ----------------------------------------------------------------------------
// VoxdotTerrain Class Implementation
// ----------------------------------------------------------------------------

VoxdotTerrain::VoxdotTerrain() :
		voxel_scale(0.1f) // Default initial voxel scale
{
	// Initialize noise with a default FastNoiseLite resource
	noise.instantiate();
	// Set some sensible defaults for the noise resource
	noise->set_seed(12345); // Correct method: set_seed
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN); // Correct enum: FastNoiseLite::TYPE_PERLIN
	noise->set_frequency(0.05f);
	noise->set_fractal_octaves(3);
	noise->set_fractal_lacunarity(2.0f);
	noise->set_fractal_gain(0.5f);
	noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM); // Correct enum: FastNoiseLite::FRACTAL_FBM

	noise_max = 0.1;
	noise_base = 0.5;

	// Ensure shared_material is instantiated in the constructor
	if (shared_material.is_null()) {
		shared_material.instantiate();
		// Set some sensible defaults if not set by inspector
		/*if (shared_material == StandardMaterial3D) {
			shared_material->set_albedo(Color(1.0, 1.0, 1.0));
			shared_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
			shared_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		}*/
		
	}


	_voxel_type_properties.resize(256); // Resize the Godot Vector
	for (int i = 0; i < 256; ++i) {
		Ref<VoxelMaterialProperties> props;
		props.instantiate(); // Create a new instance of the resource
		_voxel_type_properties.set(i, props); // Assign it to the Vector element
	}
}

VoxdotTerrain::~VoxdotTerrain() {
	// Call cleanup system in destructor
	cleanup_terrain_system();
}


//void VoxdotTerrain::import_palette_png(const String &p_path) {
//	Ref<Image> palette_image = Image::load_from_file(p_path);
//
//	if (palette_image.is_null()) {
//		OS::get_singleton()->printerr("Failed to load palette image from path: ", p_path);
//		return;
//	}
//
//	if (palette_image->is_compressed()) {
//		palette_image->decompress();
//	}
//
//	// MagicaVoxel uses palette indices 1-255, which map to the first 255 colors (at 0-based index 0-254) in the palette.
//	// We load the first 255 pixels from the image to serve as the colors for materials 1-255.
//	int colors_to_load = 255;
//
//	if (palette_image->get_width() * palette_image->get_height() < colors_to_load) {
//		OS::get_singleton()->printerr("Palette image does not contain at least 255 colors. Image size: ", palette_image->get_width(), "x", palette_image->get_height());
//		return;
//	}
//
//	// In Godot 4.x, you no longer need to call lock() or unlock() for get_pixel().
//	for (int i = 0; i < colors_to_load; ++i) {
//		// 'i' represents the 0-based palette index (0-254).
//		// 'material_id' represents the 1-based voxel color index (1-255).
//		int material_id = i + 1;
//
//		int x = i % palette_image->get_width();
//		int y = i / palette_image->get_width();
//
//		Color pixel_color = palette_image->get_pixel(x, y);
//
//		// Convert Godot::Color to Godot::Vector4.
//		Vector4 material_color(pixel_color.r, pixel_color.g, pixel_color.b, pixel_color.a);
//
//		// Set the color for the corresponding material ID.
//		// Color from Pixel 0 -> Material 1
//		// Color from Pixel 1 -> Material 2
//		// ...
//		// Color from Pixel 254 -> Material 255
//		_voxel_mesher.setMaterialColor(material_id, material_color);
//	}
//
//	OS::get_singleton()->print("Successfully loaded and applied 255 palette colors from: ", p_path);
//}

void VoxdotTerrain::import_palette_png(const String &p_path) {
	Ref<Image> palette_image = Image::load_from_file(p_path);

	if (palette_image.is_null()) {
		OS::get_singleton()->printerr("Failed to load palette image from path: ", p_path);
		return;
	}

	if (palette_image->is_compressed()) {
		palette_image->decompress();
	}

	// MagicaVoxel uses palette indices 1-255, which map to the first 255 colors (at 0-based index 0-254) in the palette.
	// We load the first 255 pixels from the image to serve as the colors for materials 1-255.
	int colors_to_load = 255;

	if (palette_image->get_width() * palette_image->get_height() < colors_to_load) {
		OS::get_singleton()->printerr("Palette image does not contain at least 255 colors. Image size: ", palette_image->get_width(), "x", palette_image->get_height());
		return;
	}

	// In Godot 4.x, you no longer need to call lock() or unlock() for get_pixel().
	for (int i = 0; i < colors_to_load; ++i) {
		// 'i' represents the 0-based palette index (0-254).
		// 'material_id' represents the 1-based voxel color index (1-255).
		int material_id = i + 1;

		int x = i % palette_image->get_width();
		int y = i / palette_image->get_width();

		Color pixel_color = palette_image->get_pixel(x, y);

		// **CHANGE MADE HERE: Convert sRGB color from PNG to linear color space.**
		// This applies the sRGB EOTF (Electro-Optical Transfer Function).
		Color linear_color = pixel_color.srgb_to_linear();

		// Convert Godot::Color to Godot::Vector4.
		Vector4 material_color(linear_color.r, linear_color.g, linear_color.b, linear_color.a);

		// Set the color for the corresponding material ID.
		// Color from Pixel 0 -> Material 1
		// Color from Pixel 1 -> Material 2
		// ...
		// Color from Pixel 254 -> Material 255
		_voxel_mesher.setMaterialColor(material_id, material_color);
	}

	OS::get_singleton()->print("Successfully loaded and applied 255 palette colors from: ", p_path);
}

void VoxdotTerrain::init_terrain_system(float initial_voxel_scale, int noise_seed, int pool_size) {
	set_physics_process(true); // Ensure physics process is active for collision updates
	// Allocate memory for raw mesh data generated by mesher.cpp
	// CS is assumed to be defined in mesher.h (e.g., 62)
	const int CS_LOCAL = 62; // Assuming CS is 62 based on mesher.h snippet
	const int CS_P_LOCAL = CS_LOCAL + 2; // Padded chunk size (e.g., 64)
	const int CS_P2_LOCAL = CS_P_LOCAL * CS_P_LOCAL; // Padded chunk size squared
	const int CS_2_LOCAL = CS_LOCAL * CS_LOCAL; // Inner chunk size squared

	m_reuseable_meshdata.faceMasks = new uint64_t[CS_2_LOCAL * 6];
	m_reuseable_meshdata.opaqueMask = new uint64_t[CS_P2_LOCAL];
	m_reuseable_meshdata.forwardMerged = new uint8_t[CS_2_LOCAL];
	m_reuseable_meshdata.rightMerged = new uint8_t[CS_LOCAL];

	m_reuseable_meshdata.maxVertices = static_cast<size_t>(CS_P_LOCAL) * CS_P_LOCAL * CS_P_LOCAL * 6 / 2;
	if (m_reuseable_meshdata.maxVertices < 1024) {
		m_reuseable_meshdata.maxVertices = 1024;
	}
	// meshData.vertices is a BM_VECTOR<uint64_t>*, assumed to be std::vector<uint64_t>*
	m_reuseable_meshdata.vertices = new BM_VECTOR<uint64_t>(m_reuseable_meshdata.maxVertices);

	// Initialize allocated memory to zero
	memset(m_reuseable_meshdata.faceMasks, 0, sizeof(uint64_t) * CS_2_LOCAL * 6);
	memset(m_reuseable_meshdata.opaqueMask, 0, sizeof(uint64_t) * CS_P2_LOCAL);
	memset(m_reuseable_meshdata.forwardMerged, 0, sizeof(uint8_t) * CS_2_LOCAL);
	memset(m_reuseable_meshdata.rightMerged, 0, sizeof(uint8_t) * CS_LOCAL);


	voxel_scale = initial_voxel_scale;

	// Initialize the MeshInstance3D pool
	for (int i = 0; i < pool_size; ++i) {
		MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_visible(false); // Initially hide it
		add_child(mesh_instance); // Add to the scene but keep it inactive
		mesh_instance_pool.push_back(mesh_instance);
	}

	// Initialize the CollisionShape3D pool
	for (int i = 0; i < pool_size; ++i) { // Use the same pool size for collision shapes
		CollisionShape3D *collision_shape = memnew(CollisionShape3D);
		collision_shape->set_disabled(true); // Initially disable it
		add_child(collision_shape, true); // Add as direct child of the StaticBody3D (this)
		collision_shape->set_owner(this); // Set owner for proper scene tree management
		collision_shape_pool_free.push_back(collision_shape);
	}
	


	// You can set other noise properties here as needed
	OS::get_singleton()->print("VoxdotTerrain system initialized.\n");
}

void VoxdotTerrain::cleanup_terrain_system() {
	if (m_reuseable_meshdata.faceMasks) {
		delete[] m_reuseable_meshdata.faceMasks;
		m_reuseable_meshdata.faceMasks = nullptr;
	}
	if (m_reuseable_meshdata.opaqueMask) {
		delete[] m_reuseable_meshdata.opaqueMask;
		m_reuseable_meshdata.opaqueMask = nullptr;
	}
	if (m_reuseable_meshdata.forwardMerged) {
		delete[] m_reuseable_meshdata.forwardMerged;
		m_reuseable_meshdata.forwardMerged = nullptr;
	}
	if (m_reuseable_meshdata.rightMerged) {
		delete[] m_reuseable_meshdata.rightMerged;
		m_reuseable_meshdata.rightMerged = nullptr;
	}
	if (m_reuseable_meshdata.vertices) {
		delete m_reuseable_meshdata.vertices;
		m_reuseable_meshdata.vertices = nullptr;
	}

	clear_collision_shape_pool();

	chunk_map.clear(); // Clear all chunk metadata

	// The mesh instances are children of this node, so they will be freed
	// when this node is freed. We can just clear the pool vector.
	mesh_instance_pool.clear();
	// No other resources typically need manual cleanup for Godot objects
	OS::get_singleton()->print("VoxdotTerrain system cleaned up.\n");
}

void VoxdotTerrain::add_chunk(const Vector3 &coords, bool empty) {
	if (!chunk_map.has(coords)) {
		ChunkMetadata new_chunk_metadata;
		new_chunk_metadata.chunkCoords = coords;
		new_chunk_metadata.is_dirty = true; // Mark as dirty for initial meshing		

		if (empty == false) {
			dirty_chunks.push_back(coords); // Add to dirty list
			new_chunk_metadata.generate_terrain = true;
		} else {
			dirty_chunks.push_back(coords); // Add to dirty list
			new_chunk_metadata.generate_terrain = false;
		}
		chunk_map.insert(coords, new_chunk_metadata);
		
		//OS::get_singleton()->print(vformat("Chunk added: %s\n", coords));
	} else {
		//OS::get_singleton()->print(vformat("Chunk already exists: %s\n", coords));
	}
}

// Also update remove_chunk to use the pool.
void VoxdotTerrain::remove_chunk(const Vector3 &coords) {
	ChunkMetadata *md = chunk_map.getptr(coords);
	if (md) {
		if (md->meshInstance.is_valid()) {
			Object *obj = ObjectDB::get_instance(md->meshInstance);
			if (obj) {
				MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(obj);
				if (mesh_instance) {
					return_mesh_instance_to_pool(mesh_instance);
				}
			}
		}

		// Return the collision shape to the pool using the stored ID
		if (md->collisionShape.is_valid()) {
			Object *obj = ObjectDB::get_instance(md->collisionShape);
			if (obj) {
				CollisionShape3D *collision_shape_to_return = Object::cast_to<CollisionShape3D>(obj);
				if (collision_shape_to_return) {
					return_collision_shape_to_pool(collision_shape_to_return);
				}
			}
		}

		chunk_map.erase(coords);
	}
}


bool VoxdotTerrain::has_chunk(const Vector3 &coords) const {
	return chunk_map.has(coords);
}

void VoxdotTerrain::clear_all_chunks() {
	// Iterate through the chunk_map to free all mesh instances before clearing.
	for (const KeyValue<Vector3, ChunkMetadata> &E : chunk_map) {
		if (E.value.meshInstance.is_valid()) {
			Object *obj = ObjectDB::get_instance(E.value.meshInstance);
			if (obj) {
				Node *node_to_delete = Object::cast_to<Node>(obj);
				if (node_to_delete) {
					node_to_delete->queue_free();
				}
			}
		}
		// Also handle collision shapes
		if (E.value.collisionShape.is_valid()) {
			Object *obj = ObjectDB::get_instance(E.value.collisionShape);
			if (obj) {
				Node *node_to_delete = Object::cast_to<Node>(obj);
				if (node_to_delete) {
					node_to_delete->queue_free();
				}
			}
		}
	}
	chunk_map.clear();
	OS::get_singleton()->print("All chunks cleared.\n");
}


void VoxdotTerrain::generate_mesh_data(MeshData &outMeshData, const std::vector<uint8_t> &voxels) {
	auto mesh_start = std::chrono::high_resolution_clock::now();
	//MeshData meshData;

	// Allocate memory for raw mesh data generated by mesher.cpp
	// CS is assumed to be defined in mesher.h (e.g., 62)
	const int CS_LOCAL = 62; // Assuming CS is 62 based on mesher.h snippet
	const int CS_P_LOCAL = CS_LOCAL + 2; // Padded chunk size (e.g., 64)
	const int CS_P2_LOCAL = CS_P_LOCAL * CS_P_LOCAL; // Padded chunk size squared
	const int CS_2_LOCAL = CS_LOCAL * CS_LOCAL; // Inner chunk size squared

	/*meshData.faceMasks = new uint64_t[CS_2_LOCAL * 6];
	meshData.opaqueMask = new uint64_t[CS_P2_LOCAL];
	meshData.forwardMerged = new uint8_t[CS_2_LOCAL];
	meshData.rightMerged = new uint8_t[CS_LOCAL];*/

	outMeshData.maxVertices = static_cast<size_t>(CS_P_LOCAL) * CS_P_LOCAL * CS_P_LOCAL * 6 / 2;
	if (outMeshData.maxVertices < 1024) {
		outMeshData.maxVertices = 1024;
	}
	// meshData.vertices is a BM_VECTOR<uint64_t>*, assumed to be std::vector<uint64_t>*
	outMeshData.vertexCount = 0;
	if (outMeshData.vertices) {
		outMeshData.vertices->clear(); // Clear the existing vector
		outMeshData.vertices->resize(outMeshData.maxVertices); // or outMeshData.vertices->resize(outMeshData.vertices->capacity());
		//outMeshData.vertices = new BM_VECTOR<uint64_t>(outMeshData.maxVertices);
	} else {
		// This case should ideally not happen if init_terrain_system is called first.
		// If it does, re-instantiate, though this indicates an issue with setup flow.
		outMeshData.vertices = new BM_VECTOR<uint64_t>(outMeshData.maxVertices);
	}

	// Initialize allocated memory to zero
	memset(outMeshData.faceMasks, 0, sizeof(uint64_t) * CS_2_LOCAL * 6);
	//memset(m_reuseable_meshdata.opaqueMask, 0, sizeof(uint64_t) * CS_P2_LOCAL);
	memset(outMeshData.forwardMerged, 0, sizeof(uint8_t) * CS_2_LOCAL);
	memset(outMeshData.rightMerged, 0, sizeof(uint8_t) * CS_LOCAL);

	
	// Generate opaque mask
	for (int z = 0; z < CS_P_LOCAL; ++z) {
		for (int y = 0; y < CS_P_LOCAL; ++y) {
			uint64_t bits = 0;
			for (int x = 0; x < CS_P_LOCAL; ++x) {
				if (voxels[x + (y * CS_P_LOCAL) + (z * CS_P2_LOCAL)]) {
					bits |= (1ull << x);
				}
			}
			outMeshData.opaqueMask[y + z * CS_P_LOCAL] = bits;
		}
	}

	// Call the mesh function from mesher.cpp
	// Ensure mesher.cpp expects raw uint8_t* for voxels.
	// Godot's Vector<uint8_t>::ptr() gives a raw pointer.
	
	mesh(voxels.data(), outMeshData);
	
	// Trim the vertices vector
	if (outMeshData.vertices) {
		outMeshData.vertices->resize(outMeshData.vertexCount);
	}

	auto mesh_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::micro> mesh_duration_micro = mesh_end - mesh_start;
	std::cout << "mesh and bitmask execution time: " << mesh_duration_micro.count() << " us" << "\n";
	//return m_reuseable_meshdata;
}

Vector<Vector3> VoxdotTerrain::get_loaded_chunk_coords() const {
	Vector<Vector3> coords_list;
	// Use standard C++ iteration with HashMap's begin() and end() iterators.
	// This is generally more robust across different Godot versions/builds.
	for (HashMap<Vector3, ChunkMetadata, Vector3Hasher, Vector3Equals>::ConstIterator it = chunk_map.begin(); it != chunk_map.end(); ++it) {
		coords_list.push_back(it->key); // Access key directly from iterator dereference
	}
	return coords_list;
}




//void VoxdotTerrain::create_and_add_mesh_instance_child(
//		const GodotMeshData &godot_mesh_data,
//		const Vector3 &chunk_coords_vector,
//		float p_voxel_scale,
//		bool p_add_collision_body) { // Using p_voxel_scale to match signature
//
//	auto instance_start = std::chrono::high_resolution_clock::now();
//
//	// Check if there's actual mesh data to create a mesh from
//	if (godot_mesh_data.vertices.empty() || godot_mesh_data.indices.empty()) {
//		//OS::get_singleton()->printerr("create_and_add_mesh_instance_child: No mesh data provided. Aborting.\n");
//		return;
//	}
//
//	// 1. Create a new ArrayMesh
//	Ref<ArrayMesh> array_mesh;
//	array_mesh.instantiate(); // Or new ArrayMesh(); in older Godot versions
//
//	// 2. Prepare the arrays for the mesh
//	Array mesh_arrays;
//	mesh_arrays.resize(Mesh::ARRAY_MAX); // Resize to hold all mesh array types
//
//	// Convert std::vector to Godot's PackedVector3Array, PackedVector2Array etc.
//	PackedVector3Array vertices;
//	for (size_t i = 0; i < godot_mesh_data.vertices.size(); i += 3) {
//		vertices.push_back(Vector3(
//				godot_mesh_data.vertices[i],
//				godot_mesh_data.vertices[i + 1],
//				godot_mesh_data.vertices[i + 2]));
//	}
//	mesh_arrays[Mesh::ARRAY_VERTEX] = vertices;
//
//	if (!godot_mesh_data.normals.empty()) {
//		PackedVector3Array normals;
//		for (size_t i = 0; i < godot_mesh_data.normals.size(); i += 3) {
//			normals.push_back(Vector3(
//					godot_mesh_data.normals[i],
//					godot_mesh_data.normals[i + 1],
//					godot_mesh_data.normals[i + 2]));
//		}
//		mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
//	}
//
//	if (!godot_mesh_data.uvs.empty()) {
//		PackedVector2Array uvs;
//		for (size_t i = 0; i < godot_mesh_data.uvs.size(); i += 2) {
//			uvs.push_back(Vector2(
//					godot_mesh_data.uvs[i],
//					godot_mesh_data.uvs[i + 1]));
//		}
//		mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
//	}
//
//	if (!godot_mesh_data.colors.empty()) {
//		PackedColorArray colors;
//		for (size_t i = 0; i < godot_mesh_data.colors.size(); i += 4) {
//			colors.push_back(Color(
//					godot_mesh_data.colors[i],
//					godot_mesh_data.colors[i + 1],
//					godot_mesh_data.colors[i + 2],
//					godot_mesh_data.colors[i + 3]));
//		}
//		mesh_arrays[Mesh::ARRAY_COLOR] = colors;
//	}
//
//	PackedInt32Array indices;
//	for (size_t i = 0; i < godot_mesh_data.indices.size(); ++i) {
//		indices.push_back(godot_mesh_data.indices[i]);
//	}
//	mesh_arrays[Mesh::ARRAY_INDEX] = indices;
//
//	// 3. Add the surface to the ArrayMesh
//	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mesh_arrays);
//
//	// 4. Create a MeshInstance3D node
//	MeshInstance3D *mesh_instance = memnew(MeshInstance3D); // Godot's memory allocation
//
//	// 5. Set the mesh for the MeshInstance3D
//	mesh_instance->set_mesh(array_mesh);
//
//	// 6. Pretend function to create and set a material
//	//Ref<StandardMaterial3D> material;
//	//material.instantiate();
//	//material->set_albedo(Color(1.0, 1.0, 1.0)); // Set albedo to white or a neutral color
//	//// Enable using vertex colors for the albedo (base color) of the material
//	//material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
//	/*material->set_vertex_color_use_as_albedo(true);
//	material->vertex_color*/
//
//	mesh_instance->set_surface_override_material(0, shared_material); // Apply the material
//
//	// 7. Set the position of the MeshInstance3D based on chunk_coords_vector and p_voxel_scale
//	const float chunk_world_size = CS * p_voxel_scale; // CS from mesher.h
//	Transform3D transform;
//	transform.origin = Vector3(
//			chunk_coords_vector.x * chunk_world_size,
//			chunk_coords_vector.y * chunk_world_size,
//			chunk_coords_vector.z * chunk_world_size);
//	mesh_instance->set_transform(transform);
//
//	add_child(mesh_instance);
//
//	std::chrono::duration<double, std::micro> collision_duration_micro;
//	if (p_add_collision_body) {
//		// This single line replaces all the manual creation:
//		auto collision_start = std::chrono::high_resolution_clock::now();
//		//mesh_instance->create_trimesh_collision();
//
//		//mesh_instance->create_trimesh_collision();
//		//create_and_add_collision_shape_child(godot_mesh_data, chunk_coords_vector, voxel_scale);
//
//		auto collision_end = std::chrono::high_resolution_clock::now();
//		collision_duration_micro = collision_end - collision_start;
//		std::cout << "collision execution time: " << collision_duration_micro.count() << " us" << "\n";
//		//mesh_instance->create_convex_collision(false, false);
//		//mesh_instance->create_multiple_convex_collisions();
//	}
//
//	// Store the new mesh instance ID in the chunk's metadata.
//	ChunkMetadata *md = chunk_map.getptr(chunk_coords_vector);
//	if (md) {
//		md->meshInstance = mesh_instance->get_instance_id();
//
//	} else {
//		OS::get_singleton()->printerr("create_and_add_mesh_instance_child: Could not find ChunkMetadata for coords %s to store mesh reference.");
//	}
//
//	auto instance_end = std::chrono::high_resolution_clock::now();
//	std::chrono::duration<double, std::micro> instance_duration_micro = (instance_end - instance_start) - collision_duration_micro;
//	std::cout << "instance creation execution time: " << instance_duration_micro.count() << " us" << "\n";
//	//OS::get_singleton()->print("Created and added MeshInstance3D as child.\n");
//}
//


// CHunk management:
MeshInstance3D *VoxdotTerrain::get_mesh_instance_from_pool() {
	if (!mesh_instance_pool.is_empty()) {
		MeshInstance3D *mesh_instance = mesh_instance_pool.get(mesh_instance_pool.size() - 1);
		mesh_instance_pool.remove_at(mesh_instance_pool.size() - 1);
		mesh_instance->set_visible(true);
		return mesh_instance;
	}

	// Optional: Handle pool exhaustion by creating a new instance
	OS::get_singleton()->printerr("MeshInstance3D pool exhausted! Creating a new instance.");
	MeshInstance3D *new_mesh_instance = memnew(MeshInstance3D);
	add_child(new_mesh_instance);
	return new_mesh_instance;
}

void VoxdotTerrain::return_mesh_instance_to_pool(MeshInstance3D *mesh_instance) {
	if (mesh_instance) {
		mesh_instance->set_visible(false);
		mesh_instance->set_mesh(Ref<Mesh>()); // Clear the mesh
		// Reset transform or other properties if necessary
		mesh_instance->set_transform(Transform3D());
		mesh_instance_pool.push_back(mesh_instance);
	}
}


// collision management
// Helper functions for the collision pool
CollisionShape3D *VoxdotTerrain::get_collision_shape_from_pool(const Vector3 &p_chunk_coords) {
	CollisionShape3D *collision_shape;
	if (collision_shape_pool_free.size() > 0) {
		// Get the last element and remove it (efficient for Vector)
		collision_shape = collision_shape_pool_free.get(collision_shape_pool_free.size() - 1);
		collision_shape_pool_free.remove_at(collision_shape_pool_free.size() - 1);
		// Re-add it to the scene tree (it was removed when returned to pool)
		if (!collision_shape->is_inside_tree()) {
			add_child(collision_shape, true);
		}
	} else {
		collision_shape = memnew(CollisionShape3D);
		// Add as direct child of the StaticBody3D (this)
		add_child(collision_shape, true);
		collision_shape->set_owner(this); // Set owner for proper scene tree management
	}

	// Set a name for easier debugging in the editor
	collision_shape->set_name("CollisionShape_" + String::num_int64(p_chunk_coords.x) + "_" + String::num_int64(p_chunk_coords.y) + "_" + String::num_int64(p_chunk_coords.z));

	// Enable the collision shape when getting it from the pool
	collision_shape->set_disabled(false);
	collision_shape_pool_in_use.push_back(collision_shape); // Add to in_use vector
	return collision_shape;
}

void VoxdotTerrain::return_collision_shape_to_pool(CollisionShape3D *p_collision_shape) {
	if (!p_collision_shape) {
		return;
	}

	// Find and remove from in_use list (less efficient with Vector, but necessary if order isn't guaranteed)
	// For better performance, consider storing a boolean flag on the CollisionShape3D itself
	// indicating if it's "in use" vs "free", and just iterating the entire pool when cleaning up.
	// Or, if you have a direct mapping (e.g., in a HashMap for active chunks), remove it from there.
	int index = collision_shape_pool_in_use.find(p_collision_shape);
	if (index != -1) {
		collision_shape_pool_in_use.remove_at(index);
	}

	// Clear its shape to free memory/resources if not immediately reused
	p_collision_shape->set_shape(Ref<Shape3D>()); // Correct way to set a Ref to null
	p_collision_shape->set_disabled(true);

	if (p_collision_shape->get_parent() == this) {
		remove_child(p_collision_shape);
	}

	collision_shape_pool_free.push_back(p_collision_shape);
}

void VoxdotTerrain::clear_collision_shape_pool() {
	// Iterate and free all elements in both vectors
	for (int i = 0; i < collision_shape_pool_free.size(); ++i) {
		if (collision_shape_pool_free[i]) {
			collision_shape_pool_free[i]->queue_free();
		}
	}
	collision_shape_pool_free.clear();

	for (int i = 0; i < collision_shape_pool_in_use.size(); ++i) {
		if (collision_shape_pool_in_use[i]) {
			collision_shape_pool_in_use[i]->queue_free();
		}
	}
	collision_shape_pool_in_use.clear();
}

CollisionShape3D *VoxdotTerrain::create_and_add_collision_shape_child(const GodotMeshData &p_godot_mesh_data, const Vector3 &p_chunk_coords, float p_voxel_scale) {
	CollisionShape3D *collision_shape = get_collision_shape_from_pool(p_chunk_coords);

	Ref<ConcavePolygonShape3D> collision_mesh_shape;
	collision_mesh_shape.instantiate();

	// Create a PackedVector3Array that contains vertices in triangle order, using indices.
	PackedVector3Array triangle_vertices_for_collision;
	// The total number of Vector3 elements will be the same as the number of indices,
	// as each index points to a vertex that will be part of a triangle in the collision shape.
	triangle_vertices_for_collision.resize(p_godot_mesh_data.indices.size());

	// Populate the PackedVector3Array by fetching vertices using the indices.
	// Each set of 3 indices in p_godot_mesh_data.indices forms a triangle.
	// We iterate through the indices and add the corresponding Vector3 to triangle_vertices_for_collision.
	for (size_t i = 0; i < p_godot_mesh_data.indices.size(); ++i) {
		uint32_t vertex_index_in_source = p_godot_mesh_data.indices[i];

		// Ensure the index is valid to prevent out-of-bounds access
		if (vertex_index_in_source * 3 + 2 >= p_godot_mesh_data.vertices.size()) {
			// Handle error: index out of bounds, this should not happen if mesh data is valid
			OS::get_singleton()->printerr("Error: Vertex index out of bounds when creating collision shape.");
			// You might want to skip this triangle or break, depending on desired error handling
			// For now, let's just break to prevent a crash.
			break;
		}

		float x = p_godot_mesh_data.vertices[vertex_index_in_source * 3];
		float y = p_godot_mesh_data.vertices[vertex_index_in_source * 3 + 1];
		float z = p_godot_mesh_data.vertices[vertex_index_in_source * 3 + 2];
		triangle_vertices_for_collision.set(i, Vector3(x, y, z));
	}

	// Pass the triangle-ordered vertices to set_faces.
	// Since p_godot_mesh_data.indices.size() is guaranteed to be a multiple of 3 (for triangles),
	// triangle_vertices_for_collision.size() will also be a multiple of 3,
	// satisfying ConcavePolygonShape3D's requirements.
	collision_mesh_shape->set_faces(triangle_vertices_for_collision);

	collision_shape->set_shape(collision_mesh_shape);

	// Position the collision shape at the correct chunk offset
	// The mesh data is relative to the chunk's local origin (0,0,0)
	// The collision shape should also be at the chunk's world position.
	collision_shape->set_position(p_chunk_coords * 62 * p_voxel_scale);

	return collision_shape; // Return the created/reused collision shape
}


// New function implementation

//bool VoxdotTerrain::is_chunk_partially_filled(const Vector3 &chunk_coords, float padding) const {
//	const int INNER_CHUNK_SIZE = CS; // CS is 62
//	const int PADDED_CHUNK_SIZE = CS + 2; // 64
//
//	// Calculate the world position of the chunk's center for noise sampling
//	// This is the center of the INNER chunk, not the padded one.
//	float center_world_x = (chunk_coords.x * INNER_CHUNK_SIZE + INNER_CHUNK_SIZE / 2.0f) * voxel_scale;
//	float center_world_z = (chunk_coords.z * INNER_CHUNK_SIZE + INNER_CHUNK_SIZE / 2.0f) * voxel_scale;
//
//	// Get the terrain height in 'voxel units' (relative to world Y=0, in terms of voxel indices)
//	// The noise_base and noise_max are assumed to define height in terms of voxel units.
//	float noise_value = noise->get_noise_2d(center_world_x, center_world_z);
//	float terrain_height_voxel_units_from_origin = noise_base + (noise_value * noise_max * 2.0f);
//
//	// Calculate the chunk's vertical bounds in 'voxel units' (global voxel indices)
//	// The chunk_coords.y is the chunk index. Each chunk is INNER_CHUNK_SIZE voxels high.
//	float chunk_bottom_voxel_y = chunk_coords.y * INNER_CHUNK_SIZE;
//	float chunk_top_voxel_y = (chunk_coords.y + 1) * INNER_CHUNK_SIZE;
//
//	// Define a buffer in voxel units. This should be a few voxels to be generous
//	// and ensure we don't miss terrain due to sampling only the center.
//	const float GENEROUS_VOXEL_BUFFER = padding; // e.g., 62 voxels
//
//	// Check if the terrain height (plus/minus buffer) overlaps with the chunk's vertical voxel bounds.
//	// If the terrain's highest point (terrain_height + buffer) is below the chunk's bottom, it's air.
//	if (terrain_height_voxel_units_from_origin + GENEROUS_VOXEL_BUFFER < chunk_bottom_voxel_y) {
//		return false; // Completely air
//	}
//	// If the terrain's lowest point (terrain_height - buffer) is above the chunk's top, it's solid.
//	else if (terrain_height_voxel_units_from_origin - GENEROUS_VOXEL_BUFFER > chunk_top_voxel_y) {
//		return false; // Completely solid
//	}
//	// Otherwise, the terrain (with buffer) intersects the chunk vertically, so it's partially filled.
//	else {
//		return true; // Partially filled
//	}
//}
bool VoxdotTerrain::is_chunk_partially_filled(const Vector3 &chunk_coords, float padding) const {
	if (biomes.is_empty()) {
		// Fallback: If no biomes, assume chunks should be checked for generation.
		return true;
	}

	float global_max_height = -std::numeric_limits<float>::max();
	float global_min_height = std::numeric_limits<float>::max();
	bool has_3d_layers = false;

	// --- 1. Find the theoretical min/max height bounds of the entire world ---
	for (int i = 0; i < biomes.size(); ++i) {
		Ref<Biome> biome = biomes[i];
		if (biome.is_null()) {
			continue;
		}

		const Array &layers = biome->get_terrain_layers();
		for (int j = 0; j < layers.size(); ++j) {
			Ref<TerrainLayer> layer = layers[j];
			if (layer.is_null()) {
				continue;
			}

			if (layer->get_dimension()) {
				has_3d_layers = true;
			} else {
				// For 2D layers, calculate theoretical min/max based on noise range [-1, 1]
				const float layer_max_h = layer->get_noise_base() + layer->get_noise_max() * 2.0f;
				const float layer_min_h = layer->get_noise_base() - layer->get_noise_max() * 2.0f;
				if (layer_max_h > global_max_height) {
					global_max_height = layer_max_h;
				}
				if (layer_min_h < global_min_height) {
					global_min_height = layer_min_h;
				}
			}
		}
	}

	// If no 2D layers exist to define bounds, we must generate the chunk to be safe.
	if (global_max_height == -std::numeric_limits<float>::max()) {
		return true;
	}

	// --- 2. Compare chunk's vertical position with global height bounds ---
	const int INNER_CHUNK_SIZE = CS; // From mesher.h
	const float chunk_bottom_voxel_y = chunk_coords.y * INNER_CHUNK_SIZE;
	const float chunk_top_voxel_y = (chunk_coords.y + 1) * INNER_CHUNK_SIZE;

	// Add user-provided padding for a more generous check
	const float max_h_with_padding = global_max_height + padding;
	const float min_h_with_padding = global_min_height - padding;

	// If the chunk is entirely above the highest possible 2D terrain...
	if (chunk_bottom_voxel_y > max_h_with_padding) {
		// ...it's empty, unless a 3D layer like floating islands could exist.
		return has_3d_layers;
	}

	// If the chunk is entirely below the lowest possible 2D terrain...
	if (chunk_top_voxel_y < min_h_with_padding) {
		// ...it's solid, unless a 3D layer like caves could exist.
		return has_3d_layers;
	}

	// The chunk is within the potential terrain band, so it's considered partially filled.
	return true;
}

// ----------------------------------------------------------------------------
// VoxdotTerrain Data Management
// ----------------------------------------------------------------------------


void VoxdotTerrain::process_dirty_chunks(int max_chunks_to_process, bool process_oldest_chunks_first) {
	auto final_start = std::chrono::high_resolution_clock::now();
	int processed_count = 0;

	if (process_oldest_chunks_first) { // Check the new setting
		// Iterate forwards to process older chunks first
		for (int i = 0; i < dirty_chunks.size() && processed_count < max_chunks_to_process;) {
			Vector3 chunk_coords = dirty_chunks[i];
			ChunkMetadata *md = chunk_map.getptr(chunk_coords);

			if (md && md->is_dirty) {
				const int padded_chunk_size = CS + 2;
				recreate_chunk_mesh(chunk_coords, padded_chunk_size);
				md->is_dirty = false;
				dirty_chunks.remove_at(i); // Remove from dirty list
				processed_count++;
				// Do not increment 'i' if an element is removed, as the next element shifts into 'i'
			} else {
				// If for some reason a chunk is in dirty_chunks but not in chunk_map or not dirty, remove it
				dirty_chunks.remove_at(i);
				// Do not increment 'i' if an element is removed
			}
		}
	} else {
		// Current behavior: Iterate backwards to process newer chunks first
		for (int i = dirty_chunks.size() - 1; i >= 0 && processed_count < max_chunks_to_process; --i) {
			Vector3 chunk_coords = dirty_chunks[i];
			ChunkMetadata *md = chunk_map.getptr(chunk_coords);

			if (md && md->is_dirty) {
				const int padded_chunk_size = CS + 2;
				recreate_chunk_mesh(chunk_coords, padded_chunk_size);
				md->is_dirty = false;
				dirty_chunks.remove_at(i); // Remove from dirty list
				processed_count++;
			} else {
				// If for some reason a chunk is in dirty_chunks but not in chunk_map or not dirty, remove it
				dirty_chunks.remove_at(i);
			}
		}
	}

	auto final_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::micro> final_duration_micro = final_end - final_start;
	std::cout << "Final execution time: " << final_duration_micro.count() << " us" << "\n";
}
















void VoxdotTerrain::populate_chunk_voxels(
		std::vector<uint8_t> &voxels,
		int cs_p_val,
		int cs_p2_val,
		int cs_p3_val,
		Vector3 chunk_offset_in_voxels,
		const Vector<Ref<ISDFEdit>> &sdf_edits,
		bool generate_terrain) {
	const int pad = 1;
	const int N = cs_p_val;

	if (voxels.size() != cs_p3_val) {
		voxels.resize(cs_p3_val);
	}
	std::fill(voxels.begin(), voxels.end(), 0);

	if (generate_terrain) {
		if (biomes.is_empty()) {
			return;
		}

		const int pad = 1;
		const int N = cs_p_val;
		const int N2 = cs_p2_val;
		const float biome_freq_multiplier = 0.05f;

		// --- 1. Biome Selection (Chunk-Center Approximation) ---
		const float centerX = (chunk_offset_in_voxels.x + (N / 2) - pad) * voxel_scale;
		const float centerZ = (chunk_offset_in_voxels.z + (N / 2) - pad) * voxel_scale;
		const float biome_selection_noise = noise->get_noise_2d(centerX * biome_freq_multiplier, centerZ * biome_freq_multiplier);
		int biome_index = floor((biome_selection_noise * 0.5f + 0.5f) * biomes.size());
		biome_index = CLAMP(biome_index, 0, biomes.size() - 1);

		const Ref<Biome> biome = biomes[biome_index];
		if (biome.is_null() || biome->get_terrain_layers().size() == 0) {
			return;
		}
		const Array &layers = biome->get_terrain_layers();

		// --- 2. Pre-compute World Coordinates ---
		std::vector<float> worldX_array;
		std::vector<float> worldZ_array;
		std::vector<float> worldY_array;

		worldX_array.reserve(N);
		worldZ_array.reserve(N);
		worldY_array.reserve(N);

		for (int i = 0; i < N; ++i) {
			worldX_array.push_back((chunk_offset_in_voxels.x + i - pad) * voxel_scale);
			worldZ_array.push_back((chunk_offset_in_voxels.z + i - pad) * voxel_scale);
			worldY_array.push_back((chunk_offset_in_voxels.y + i - pad) * voxel_scale);
		}

		// --- 3. Generate Heightmaps for 2D Layers Only ---
		std::vector<std::vector<float>> heightmaps;
		std::vector<int> layer_2d_indices;
		std::vector<Ref<TerrainLayer>> valid_layers;

		// Separate 2D and 3D layers
		for (int i = 0; i < layers.size(); ++i) {
			Ref<TerrainLayer> layer = layers[i];
			if (layer.is_valid()) {
				valid_layers.push_back(layer);

				if (!layer->get_dimension()) {
					// 2D layer - pre-generate heightmap
					std::vector<float> heightmap;
					heightmap.reserve(N * N);

					const float noise_base = layer->get_noise_base();
					const float noise_scale = layer->get_noise_max() * 2.0f;

					for (int x = 0; x < N; ++x) {
						for (int z = 0; z < N; ++z) {
							const float noise_val = layer->get_noise()->get_noise_2d(worldX_array[x], worldZ_array[z]);
							heightmap.push_back(noise_base + (noise_val * noise_scale));
						}
					}

					heightmaps.push_back(std::move(heightmap));
					layer_2d_indices.push_back(valid_layers.size() - 1);
				} else {
					// 3D layer - just add empty heightmap as placeholder
					heightmaps.push_back(std::vector<float>());
					layer_2d_indices.push_back(-1); // Mark as 3D layer
				}
			}
		}

		// --- 4. Fill Voxels with Optimized Loop ---
		for (int y = 0; y < N; ++y) {
			const float worldY_scaled = worldY_array[y];
			const size_t y_offset = (size_t)y * N2;

			for (int x = 0; x < N; ++x) {
				const float worldX = worldX_array[x];
				const size_t xy_offset = y_offset + (size_t)x * N;

				for (int z = 0; z < N; ++z) {
					uint8_t final_material = 0;

					// Process layers from top-down (reverse order)
					for (int layer_idx = valid_layers.size() - 1; layer_idx >= 0; --layer_idx) {
						const Ref<TerrainLayer> &layer = valid_layers[layer_idx];
						bool is_solid = false;

						if (layer_2d_indices[layer_idx] >= 0) {
							// 2D layer - fast heightmap lookup
							const std::vector<float> &heightmap = heightmaps[layer_idx];
							if (!heightmap.empty()) {
								const float height = heightmap[x * N + z];
								is_solid = (worldY_scaled < height);
							}
						} else {
							// 3D layer - per-voxel calculation
							const float worldZ = worldZ_array[z];
							const float noise_val = layer->get_noise()->get_noise_3d(worldX, worldY_scaled, worldZ);
							const float density = layer->get_noise_base() + noise_val * layer->get_noise_max();
							is_solid = (density > 0.0f);
						}

						if (is_solid) {
							final_material = layer->get_material_type();
							break; // Early exit
						}
					}

					if (final_material != 0) {
						const size_t index = xy_offset + z;
						voxels[index] = final_material;
					}
				}
			}
		}
	}


	//if (generate_terrain) {
	//	Vector<int> heightMap;
	//	heightMap.resize(N * N);
	//	// Removed static_cast<float>(N) multiplication here
	//	const float maxHeightGlobal = noise_max;
	//	const float baseHeightGlobal = noise_base;
	//	const int terrainMaterialType = 1;
	//	for (int z = 0; z < N; ++z) {
	//		for (int x = 0; x < N; ++x) {
	//			float worldX_noise = (chunk_offset_in_voxels.x + (x - pad)) * voxel_scale;
	//			float worldZ_noise = (chunk_offset_in_voxels.z + (z - pad)) * voxel_scale;
	//			// Use the correct method for Godot's FastNoiseLite
	//			float noiseValue = noise->get_noise_2d(worldX_noise, worldZ_noise);
	//			float terrainHeightF = baseHeightGlobal + (noiseValue * maxHeightGlobal * 2.0f);
	//			heightMap.write[x + z * N] = static_cast<int>(Math::floor(terrainHeightF));
	//		}
	//	}
	//	for (int y = 0; y < N; ++y) {
	//		const int worldY_voxel = chunk_offset_in_voxels.y + (y - pad);
	//		const size_t y_stride = static_cast<size_t>(y) * cs_p2_val;
	//		for (int x = 0; x < N; ++x) {
	//			const size_t x_stride = static_cast<size_t>(x) * cs_p_val;
	//			for (int z = 0; z < N; ++z) {
	//				if (worldY_voxel < heightMap[x + z * N]) {
	//					voxels[z + x_stride + y_stride] = terrainMaterialType;
	//				}
	//			}
	//		}
	//	}
	//}

	if (sdf_edits.is_empty()) {
		return;
	}

	const float inv_voxel_scale = 1.0f / voxel_scale;
	const Vector3 chunk_offset_f(chunk_offset_in_voxels.x, chunk_offset_in_voxels.y, chunk_offset_in_voxels.z);

	for (int i = 0; i < sdf_edits.size(); ++i) {
		Ref<ISDFEdit> edit_ptr = sdf_edits[i];
		if (edit_ptr.is_null()) {
			continue;
		}
		std::pair<Vector3, Vector3> worldBounds = edit_ptr->getApproximateWorldBounds();
		Vector3 localMin_f = (worldBounds.first * inv_voxel_scale) - chunk_offset_f;
		Vector3 localMax_f = (worldBounds.second * inv_voxel_scale) - chunk_offset_f;

		Vector3 localMin = Vector3(
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::floor(localMin_f.x) - pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::floor(localMin_f.y) - pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::floor(localMin_f.z) - pad)));
		Vector3 localMax = Vector3(
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::ceil(localMax_f.x) + pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::ceil(localMax_f.y) + pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::ceil(localMax_f.z) + pad)));

		int local_min_x = static_cast<int>(localMin.x);
		int local_min_y = static_cast<int>(localMin.y);
		int local_min_z = static_cast<int>(localMin.z);
		int local_max_x = static_cast<int>(localMax.x);
		int local_max_y = static_cast<int>(localMax.y);
		int local_max_z = static_cast<int>(localMax.z);

		for (int y = local_min_y; y <= local_max_y; ++y) {
			const float worldY_pos_scaled = (chunk_offset_in_voxels.y + y - pad) * voxel_scale;
			
			const size_t y_stride = static_cast<size_t>(y) * cs_p2_val;
			for (int x = local_min_x; x <= local_max_x; ++x) {
				const float worldX_pos_scaled = (chunk_offset_in_voxels.x + x - pad) * voxel_scale;
				const size_t x_stride = static_cast<size_t>(x) * cs_p_val;
				for (int z = local_min_z; z <= local_max_z; ++z) {
					Vector3 worldVoxelPos_scaled = { worldX_pos_scaled, worldY_pos_scaled, (chunk_offset_in_voxels.z + z - pad) * voxel_scale };
					if (edit_ptr->getSignedDistance(worldVoxelPos_scaled) <= 0.0f) {
						voxels[z + x_stride + y_stride] = edit_ptr->getMaterial(worldVoxelPos_scaled);
					}
				}
			}
		}
	}
}

void VoxdotTerrain::update_chunk_voxels(
		std::vector<uint8_t> &voxels,
		int cs_p_val, // Padded chunk size, e.g., 64
		int cs_p2_val, // Padded chunk size squared
		int cs_p3_val, // Padded chunk size cubed
		Vector3 chunk_offset_in_voxels // World-voxel offset for the chunk
) {
	ChunkMetadata *md = chunk_map.getptr(chunk_offset_in_voxels / Vector3(62, 62, 62));
	if (!md) {
		// Should not happen if called from a valid context, but good to have a safeguard.
		return;
	}

	Vector<Ref<ISDFEdit>> &new_sdf_edits = md->UnprocessedEdits;
	if (new_sdf_edits.is_empty()) {
		return;
	}

	const int pad = 1;
	const int N = cs_p_val;
	const float inv_voxel_scale = 1.0f / voxel_scale; // Use member voxel_scale
	const Vector3 chunk_offset_f(chunk_offset_in_voxels.x, chunk_offset_in_voxels.y, chunk_offset_in_voxels.z); // Convert to float Vector3

	// Process all new edits from the queue.
	for (int i = 0; i < new_sdf_edits.size(); ++i) {
		Ref<ISDFEdit> edit_ptr = new_sdf_edits[i];
		if (edit_ptr.is_null()) {
			continue;
		}

		std::pair<Vector3, Vector3> worldBounds = edit_ptr->getApproximateWorldBounds();
		Vector3 localMin_f = (worldBounds.first * inv_voxel_scale) - chunk_offset_f;
		Vector3 localMax_f = (worldBounds.second * inv_voxel_scale) - chunk_offset_f;

		Vector3 localMin = Vector3(
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::floor(localMin_f.x) - pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::floor(localMin_f.y) - pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::floor(localMin_f.z) - pad)));
		Vector3 localMax = Vector3(
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::ceil(localMax_f.x) + pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::ceil(localMax_f.y) + pad)),
				std::max(0.0f, std::min(static_cast<float>(N - 1), Math::ceil(localMax_f.z) + pad)));

		int local_min_x = static_cast<int>(localMin.x);
		int local_min_y = static_cast<int>(localMin.y);
		int local_min_z = static_cast<int>(localMin.z);

		int local_max_x = static_cast<int>(localMax.x);
		int local_max_y = static_cast<int>(localMax.y);
		int local_max_z = static_cast<int>(localMax.z);

		for (int y = local_min_y; y <= local_max_y; ++y) {
			const float worldY_pos_scaled = (chunk_offset_in_voxels.y + y - pad) * voxel_scale;
			const size_t y_stride = static_cast<size_t>(y) * cs_p2_val;
			for (int x = local_min_x; x <= local_max_x; ++x) {
				const float worldX_pos_scaled = (chunk_offset_in_voxels.x + x - pad) * voxel_scale;
				const size_t x_stride = static_cast<size_t>(x) * cs_p_val;
				for (int z = local_min_z; z <= local_max_z; ++z) {
					Vector3 worldVoxelPos_scaled = {
						worldX_pos_scaled,
						worldY_pos_scaled,
						(chunk_offset_in_voxels.z + z - pad) * voxel_scale
					};

					if (edit_ptr->getSignedDistance(worldVoxelPos_scaled) <= 0.0f) {
						voxels[z + x_stride + y_stride] = edit_ptr->getMaterial(worldVoxelPos_scaled);
					}
				}
			}
		}

		// Move the now-processed edit to the main list.
		md->sdfEdits.push_back(edit_ptr);
	}

	// Safely clear the unprocessed list now that the loop is finished.
	new_sdf_edits.clear();
}


void VoxdotTerrain::generate_godot_mesh_for_chunk(GodotMeshData &outGodotMeshData, const Vector3 &chunk_coords, int chunk_size_in_voxels) {
	/*std::ostringstream oss_start;
	oss_start << "generate_godot_mesh_for_chunk called for chunk: " << chunk_coords.x << "," << chunk_coords.y << "," << chunk_coords.z;
	OS::get_singleton()->print(oss_start.str().c_str());*/

	// These constants should match those expected by your mesher.h and generate_voxels_with_sdf
	const int CS_P_VAL = chunk_size_in_voxels; // e.g., 64
	const int CS_P2_VAL = CS_P_VAL * CS_P_VAL;
	const int CS_P3_VAL = CS_P_VAL * CS_P_VAL * CS_P_VAL;


	// Retrieve SDF edits for this chunk from the chunk_map
	ChunkMetadata *md = chunk_map.getptr(chunk_coords);
	Vector<Ref<ISDFEdit>> chunk_sdf_edits;
	if (md) {
		chunk_sdf_edits = md->sdfEdits;
	} else {
		/*std::ostringstream oss_chunk_not_found;
		oss_chunk_not_found << "generate_godot_mesh_for_chunk: Chunk " << chunk_coords.x << "," << chunk_coords.y << "," << chunk_coords.z
							<< " not found in map. Returning empty mesh data.";
		OS::get_singleton()->printerr(oss_chunk_not_found.str().c_str());*/
		return;
	}

	// Calculate the world-voxel offset for this chunk
	// (CS_P_VAL - 2) is the inner chunk size (CS = 62)
	Vector3 chunk_offset_in_voxels = chunk_coords * (CS_P_VAL - 2);

	// 1. Generate voxel data for this chunk
	auto gen_start = std::chrono::high_resolution_clock::now();
	// BUGFIX: Reworked logic to handle fresh and existing chunks correctly.
	if (md->fresh) {
		process_structures_for_chunk(chunk_coords);
		// If it's the first time, populate with base terrain and any edits already in the main list.
		// A brand new chunk will have an empty sdfEdits list here.
		populate_chunk_voxels(md->voxels, CS_P_VAL, CS_P2_VAL, CS_P3_VAL, chunk_offset_in_voxels, md->sdfEdits, md->generate_terrain);
		md->fresh = false; // It's no longer fresh after this.
	}

	// Now, *always* apply any new, unprocessed edits to the current voxel state.
	if (!md->UnprocessedEdits.is_empty()) {
		update_chunk_voxels(md->voxels, CS_P_VAL, CS_P2_VAL, CS_P3_VAL, chunk_offset_in_voxels);
	}
	
	auto gen_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::micro> gen_duration_micro = gen_end - gen_start;
	std::cout << "Generator execution time: " << gen_duration_micro.count() << " us" << std::endl;
	/*std::ostringstream oss_voxel_gen_complete;
	oss_voxel_gen_complete << "Voxel generation complete for chunk " << chunk_coords.x << "," << chunk_coords.y << "," << chunk_coords.z
						   << ". Voxels vector size: " << voxels.size();
	OS::get_singleton()->print(oss_voxel_gen_complete.str().c_str());*/

	// Quick check if voxels actually contain any data
	
	
	
	// 2. Generate raw mesh data using your existing mesher.cpp
	
	generate_mesh_data(m_reuseable_meshdata, md->voxels);
	/*std::ostringstream oss_raw_mesh_data;
	oss_raw_mesh_data << "Raw mesh data generated for chunk " << chunk_coords.x << "," << chunk_coords.y << "," << chunk_coords.z
					  << ": vertexCount=" << raw_mesh_data.vertexCount;
	OS::get_singleton()->print(oss_raw_mesh_data.str().c_str());*/

	// 3. Convert raw mesh data to GodotMeshData using MeshConverter
	
	// You might want to set material colors on the mesher if needed:
	// mesher.setMaterialColor(1, Vector4(1.0f, 0.0f, 0.0f, 1.0f)); // Example for material type 1
	auto convert_start = std::chrono::high_resolution_clock::now();
	_voxel_mesher.convertQuadsToGodotMesh(outGodotMeshData, m_reuseable_meshdata, voxel_scale, _voxel_type_properties); // Use member voxel_scale
	auto convert_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::micro> convert_duration_micro = convert_end - convert_start;
	std::cout << "Converter execution time: " << convert_duration_micro.count() << " us" << "\n";
	/*std::ostringstream oss_godot_mesh_data;
	oss_godot_mesh_data << "Godot mesh data converted for chunk " << chunk_coords.x << "," << chunk_coords.y << "," << chunk_coords.z
						<< ": vertices.size()=" << godot_mesh_data.vertices.size() << ", indices.size()=" << godot_mesh_data.indices.size();
	OS::get_singleton()->print(oss_godot_mesh_data.str().c_str());*/

	// 4. Clean up the raw_mesh_data (allocated with new/new[])
	// These were allocated with `new`, so they need `delete[]` or `delete`
	/*if (raw_mesh_data.faceMasks) {
		delete[] raw_mesh_data.faceMasks;
		raw_mesh_data.faceMasks = nullptr;
	}
	if (raw_mesh_data.opaqueMask) {
		delete[] raw_mesh_data.opaqueMask;
		raw_mesh_data.opaqueMask = nullptr;
	}
	if (raw_mesh_data.forwardMerged) {
		delete[] raw_mesh_data.forwardMerged;
		raw_mesh_data.forwardMerged = nullptr;
	}
	if (raw_mesh_data.rightMerged) {
		delete[] raw_mesh_data.rightMerged;
		raw_mesh_data.rightMerged = nullptr;
	}
	if (raw_mesh_data.vertices) {
		delete raw_mesh_data.vertices;
		raw_mesh_data.vertices = nullptr;
	}*/

	return;
}









void VoxdotTerrain::add_edit_wrapper(Vector3 size, Vector3 world_pos, int material, int shape) {
	if (shape == 0) {
		Ref<SDFSphereEdit> sphere_edit_A;
		sphere_edit_A.instantiate(); // Godot's way to create a RefCounted object
		sphere_edit_A->center = world_pos; // Set center directly
		sphere_edit_A->radius = size.x; // Set radius directly
		sphere_edit_A->material = material;

		add_sdf_edit_at_world_pos(sphere_edit_A, 64);
	}
	if (shape == 1) {
		Ref<SDFCubeEdit> cube_edit_A;
		cube_edit_A.instantiate(); // Godot's way to create a RefCounted object
		cube_edit_A->center = Vector3(world_pos); // Set center directly
		cube_edit_A->halfExtents = size; // Set halfExtents directly
		cube_edit_A->material = material;

		add_sdf_edit_at_world_pos(cube_edit_A, 64);
	}
}


//void VoxdotTerrain::preload_vox_model(const String &p_key, const String &p_path) {
//	// 1. Prevent overwriting an existing key to avoid mistakes.
//	if (_vox_model_cache.has(p_key)) {
//		OS::get_singleton()->printerr("VoxdotTerrain: A model with key '", p_key, "' is already preloaded.");
//		return;
//	}
//
//	// 2. Perform the one-time processing of the .vox file.
//	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
//	if (file.is_null()) {
//		OS::get_singleton()->printerr("preload_vox_model: Failed to open .vox file at path: %s", p_path);
//		return;
//	}
//
//	PackedByteArray buffer = file->get_buffer(file->get_length());
//	const ogt_vox_scene *scene = ogt_vox_read_scene(buffer.ptr(), buffer.size());
//
//	if (!scene) {
//		OS::get_singleton()->printerr("preload_vox_model: Failed to parse .vox scene from file: %s", p_path);
//		return;
//	}
//
//	Ref<SharedVoxData> shared_data;
//	shared_data.instantiate();
//
//	// --- This is the same processing logic from the previous answer ---
//	// It calculates the model's bounds and populates the dense voxel grid.
//	// The result is stored in the `shared_data` object.
//	// (Pasting the full logic here for completeness)
//
//	Vector3 min_voxel_coord = Vector3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
//	Vector3 max_voxel_coord = Vector3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
//
//	for (uint32_t i = 0; i < scene->num_instances; ++i) {
//		const ogt_vox_instance *instance = &scene->instances[i];
//		const ogt_vox_model *model = scene->models[instance->model_index];
//		if (!model) {
//			continue;
//		}
//
//		Vector3 instance_translation_godot = Vector3(instance->transform.m30, instance->transform.m32, instance->transform.m31);
//		Vector3 model_size_godot = Vector3(model->size_x, model->size_z, model->size_y);
//
//		min_voxel_coord = min_voxel_coord.min(instance_translation_godot);
//		max_voxel_coord = max_voxel_coord.max(instance_translation_godot + model_size_godot);
//	}
//
//	shared_data->min_corner = min_voxel_coord;
//	shared_data->dimensions = max_voxel_coord - min_voxel_coord;
//
//	size_t total_voxels = static_cast<size_t>(shared_data->dimensions.x) * static_cast<size_t>(shared_data->dimensions.y) * static_cast<size_t>(shared_data->dimensions.z);
//	if (total_voxels == 0 && scene->num_models > 0) {
//		// Handle case for single voxel models or other edge cases
//		total_voxels = 1;
//		if (shared_data->dimensions.x == 0) {
//			shared_data->dimensions.x = 1;
//		}
//		if (shared_data->dimensions.y == 0) {
//			shared_data->dimensions.y = 1;
//		}
//		if (shared_data->dimensions.z == 0) {
//			shared_data->dimensions.z = 1;
//		}
//	}
//
//	shared_data->voxels.assign(total_voxels, 0);
//	for (uint32_t i = 0; i < scene->num_instances; ++i) {
//		const ogt_vox_instance *instance = &scene->instances[i];
//		const ogt_vox_model *model = scene->models[instance->model_index];
//		if (!model) {
//			continue;
//		}
//
//		Vector3 instance_translation_godot = Vector3(instance->transform.m30, instance->transform.m32, instance->transform.m31);
//
//		for (uint32_t z = 0; z < model->size_z; ++z) {
//			for (uint32_t y = 0; y < model->size_y; ++y) {
//				for (uint32_t x = 0; x < model->size_x; ++x) {
//					uint32_t model_voxel_index = x + (y * model->size_x) + (z * model->size_x * model->size_y);
//					uint8_t material_index = model->voxel_data[model_voxel_index];
//
//					if (material_index == 0) {
//						continue; // Skip empty voxels
//					}
//
//					// Voxel's position relative to its model's origin, converted to Godot coords
//					Vector3 voxel_pos_in_model_godot = Vector3(x, z, y);
//
//					// Voxel's absolute world position (within the combined model)
//					Vector3 voxel_pos_absolute = instance_translation_godot + voxel_pos_in_model_godot;
//
//					// Position relative to our dense grid's origin
//					Vector3 grid_local_pos = voxel_pos_absolute - shared_data->min_corner;
//
//					int grid_x = static_cast<int>(grid_local_pos.x);
//					int grid_y = static_cast<int>(grid_local_pos.y);
//					int grid_z = static_cast<int>(grid_local_pos.z);
//
//					// Calculate 1D index for our flat std::vector
//					size_t grid_index = grid_x + (grid_y * static_cast<int>(shared_data->dimensions.x)) + (grid_z * static_cast<int>(shared_data->dimensions.x) * static_cast<int>(shared_data->dimensions.y));
//
//					if (grid_index < total_voxels) {
//						shared_data->voxels[grid_index] = material_index;
//					}
//				}
//			}
//		}
//	}
//
//	// --- End of processing logic ---
//
//	ogt_vox_destroy_scene(scene);
//
//	// 3. Store the processed data in the cache with the user-defined key.
//	_vox_model_cache[p_key] = shared_data;
//	OS::get_singleton()->print("Preloaded model '", p_path, "' with key '", p_key, "'.");
//}
void VoxdotTerrain::preload_vox_model(const String &p_key, const String &p_path) {
	// 1. Prevent overwriting an existing key to avoid mistakes.
	if (_vox_model_cache.has(p_key)) {
		OS::get_singleton()->printerr("VoxdotTerrain: A model with key '", p_key, "' is already preloaded.");
		return;
	}

	// 2. Perform the one-time processing of the .vox file.
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		OS::get_singleton()->printerr("preload_vox_model: Failed to open .vox file at path: %s", p_path);
		return;
	}

	PackedByteArray buffer = file->get_buffer(file->get_length());
	const ogt_vox_scene *scene = ogt_vox_read_scene(buffer.ptr(), buffer.size());

	if (!scene) {
		OS::get_singleton()->printerr("preload_vox_model: Failed to parse .vox scene from file: %s", p_path);
		return;
	}

	Ref<SharedVoxData> shared_data;
	shared_data.instantiate();

	// Calculate the overall bounds of the combined scene
	Vector3 min_voxel_coord = Vector3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	Vector3 max_voxel_coord = Vector3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

	// First pass: Determine the overall bounding box of all instances
	for (uint32_t i = 0; i < scene->num_instances; ++i) {
		const ogt_vox_instance *instance = &scene->instances[i];
		const ogt_vox_model *model = scene->models[instance->model_index];
		if (!model) {
			continue;
		}

		// Construct Godot Transform3D from ogt_vox_transform
		// ogt_vox_transform is row-major, Godot Transform3D is column-major for basis.
		// ogt_vox_transform:
		// m00 m01 m02 m03 (x_axis.x, y_axis.x, z_axis.x, translation.x)
		// m10 m11 m12 m13 (x_axis.y, y_axis.y, z_axis.y, translation.y)
		// m20 m21 m22 m23 (x_axis.z, y_axis.z, z_axis.z, translation.z)
		// Godot Transform3D basis is (x_axis, y_axis, z_axis)
		// Godot uses Y-up, Z-forward. MagicaVoxel uses Y-forward, Z-up.
		// We need to swap Y and Z components and potentially negate for coordinate system differences.
		// A common conversion for MagicaVoxel (X,Y,Z) to Godot (X,-Z,Y) or (X,Z,-Y)
		// Let's assume (X,Y,Z) in MagicaVoxel maps to (X, Z, -Y) in Godot for now,
		// and adjust if models appear rotated.
		// The ogt_vox_transform matrix is 4x3, but effectively 3x3 rotation/scale + 3x1 translation.
		// m00 m01 m02 | m03
		// m10 m11 m12 | m13
		// m20 m21 m22 | m23

		// Direct mapping of ogt_vox_transform to Godot's Transform3D,
		// assuming ogt_vox_transform uses a standard right-handed coordinate system.
		// We need to be careful with axis mapping (Y-up vs Z-up).
		// For MagicaVoxel (X, Y, Z) where Y is depth and Z is height,
		// to Godot (X, Y, Z) where Y is height and Z is depth:
		// MagicaVoxel X -> Godot X
		// MagicaVoxel Y -> Godot Z
		// MagicaVoxel Z -> Godot Y

		Transform3D instance_transform;
		// Basis (rotation and scale)
		// ogt_vox_transform is row-major. Godot basis vectors are columns.
		// So, ogt_vox_transform.m00, m10, m20 form Godot's X axis.
		// ogt_vox_transform.m01, m11, m21 form Godot's Y axis.
		// ogt_vox_transform.m02, m12, m22 form Godot's Z axis.

		// MagicaVoxel's X-axis maps to Godot's X-axis
		instance_transform.basis.set_column(0, Vector3(instance->transform.m00, instance->transform.m10, instance->transform.m20));
		// MagicaVoxel's Y-axis (depth) maps to Godot's Z-axis (depth)
		instance_transform.basis.set_column(2, Vector3(instance->transform.m01, instance->transform.m11, instance->transform.m21));
		// MagicaVoxel's Z-axis (height) maps to Godot's Y-axis (height)
		instance_transform.basis.set_column(1, Vector3(instance->transform.m02, instance->transform.m12, instance->transform.m22));

		// Origin (translation)
		// MagicaVoxel's translation X -> Godot's translation X
		// MagicaVoxel's translation Y -> Godot's translation Z
		// MagicaVoxel's translation Z -> Godot's translation Y
		instance_transform.origin = Vector3(instance->transform.m03, instance->transform.m23, instance->transform.m13);

		// Calculate the transformed corners of the model to get its world-space AABB
		Vector3 model_min_local = Vector3(0, 0, 0);
		Vector3 model_max_local = Vector3(model->size_x, model->size_y, model->size_z); // ogt_vox reports Y as depth, Z as height

		// Convert model_max_local to Godot's coordinate system for bounds calculation
		Vector3 model_max_local_godot_coords = Vector3(model->size_x, model->size_z, model->size_y);

		// Transform the local bounding box corners by the instance transform
		// We need to transform all 8 corners and find the new min/max
		AABB transformed_aabb;
		transformed_aabb.position = instance_transform.xform(model_min_local);
		transformed_aabb.size = instance_transform.xform(model_max_local_godot_coords) - transformed_aabb.position;
		transformed_aabb = transformed_aabb.abs(); // Ensure positive size

		// Update overall scene bounds
		if (i == 0) { // Initialize with the first instance's bounds
			min_voxel_coord = transformed_aabb.position;
			max_voxel_coord = transformed_aabb.position + transformed_aabb.size;
		} else {
			min_voxel_coord = min_voxel_coord.min(transformed_aabb.position);
			max_voxel_coord = max_voxel_coord.max(transformed_aabb.position + transformed_aabb.size);
		}
	}

	shared_data->min_corner = min_voxel_coord;
	shared_data->dimensions = (max_voxel_coord - min_voxel_coord).ceil(); // Use ceil to ensure dimensions are large enough

	size_t total_voxels = static_cast<size_t>(shared_data->dimensions.x) * static_cast<size_t>(shared_data->dimensions.y) * static_cast<size_t>(shared_data->dimensions.z);
	if (total_voxels == 0) {
		// Handle case where dimensions might be zero (e.g., empty scene or single point)
		// Ensure a minimum size to avoid zero-sized vector
		shared_data->dimensions = Vector3(1, 1, 1);
		total_voxels = 1;
	}

	shared_data->voxels.assign(total_voxels, 0);

	// Second pass: Populate the dense voxel grid with transformed voxel data
	for (uint32_t i = 0; i < scene->num_instances; ++i) {
		const ogt_vox_instance *instance = &scene->instances[i];
		const ogt_vox_model *model = scene->models[instance->model_index];
		if (!model) {
			continue;
		}

		// Reconstruct the instance's Godot Transform3D
		Transform3D instance_transform;
		instance_transform.basis.set_column(0, Vector3(instance->transform.m00, instance->transform.m10, instance->transform.m20));
		instance_transform.basis.set_column(2, Vector3(instance->transform.m01, instance->transform.m11, instance->transform.m21));
		instance_transform.basis.set_column(1, Vector3(instance->transform.m02, instance->transform.m12, instance->transform.m22));
		instance_transform.origin = Vector3(instance->transform.m03, instance->transform.m23, instance->transform.m13);

		for (uint32_t z = 0; z < model->size_z; ++z) { // ogt_vox Z is height
			for (uint32_t y = 0; y < model->size_y; ++y) { // ogt_vox Y is depth
				for (uint32_t x = 0; x < model->size_x; ++x) { // ogt_vox X is width
					uint32_t model_voxel_index = x + (y * model->size_x) + (z * model->size_x * model->size_y);
					uint8_t material_index = model->voxel_data[model_voxel_index];

					if (material_index == 0) {
						continue; // Skip empty voxels
					}

					// Voxel's position relative to its model's origin in MagicaVoxel coords
					Vector3 voxel_pos_in_model_magica = Vector3(x, y, z);

					// Transform voxel_pos_in_model_magica to Godot's coordinate system (X, Z, -Y)
					// This is crucial for correct rotation/positioning if MagicaVoxel's axes
					// don't directly map to Godot's.
					// However, since we are applying the instance_transform which *should* handle
					// the coordinate system conversion, we can likely just use the raw x,y,z
					// and let the transform do the work.
					// Let's use the Godot-friendly local voxel position (X, Y, Z) where Y is height.
					Vector3 local_voxel_pos_godot = Vector3(x, z, y); // Map MagicaVoxel (X,Y,Z) to Godot (X,Z,Y)

					// Apply the instance's full transform to get the voxel's absolute world position
					Vector3 voxel_pos_absolute = instance_transform.xform(local_voxel_pos_godot);

					// Position relative to our dense grid's origin
					Vector3 grid_local_pos = voxel_pos_absolute - shared_data->min_corner;

					// Round to nearest integer for grid coordinates
					int grid_x = static_cast<int>(Math::round(grid_local_pos.x));
					int grid_y = static_cast<int>(Math::round(grid_local_pos.y));
					int grid_z = static_cast<int>(Math::round(grid_local_pos.z));

					// Check bounds to prevent out-of-range access
					if (grid_x >= 0 && grid_x < shared_data->dimensions.x &&
							grid_y >= 0 && grid_y < shared_data->dimensions.y &&
							grid_z >= 0 && grid_z < shared_data->dimensions.z) {
						// Calculate 1D index for our flat std::vector
						size_t grid_index = grid_x + (grid_y * static_cast<int>(shared_data->dimensions.x)) + (grid_z * static_cast<int>(shared_data->dimensions.x) * static_cast<int>(shared_data->dimensions.y));

						if (grid_index < total_voxels) {
							shared_data->voxels[grid_index] = material_index;
						} else {
							OS::get_singleton()->printerr("VoxdotTerrain: Calculated grid_index out of bounds during voxel population.");
						}
					} else {
						OS::get_singleton()->printerr("VoxdotTerrain: Voxel position out of calculated grid bounds: ", grid_local_pos);
					}
				}
			}
		}
	}

	ogt_vox_destroy_scene(scene);

	// 3. Store the processed data in the cache with the user-defined key.
	_vox_model_cache[p_key] = shared_data;
	OS::get_singleton()->print("Preloaded model '", p_path, "' with key '", p_key, "'.");
}

void VoxdotTerrain::add_vox_edit_wrapper(String key, Vector3 world_pos, int material) {
	Ref<SDFVoxEdit> vox_edit;
	vox_edit.instantiate();
	vox_edit->set_offset(world_pos);
	vox_edit->set_material(material);
	vox_edit->set_model_key(key); // Use the key
	vox_edit->set_scale(get_voxel_scale());

	add_sdf_edit_at_world_pos(vox_edit, 64);
}

bool VoxdotTerrain::add_sdf_edit_at_world_pos(const Ref<ISDFEdit> &edit, int chunk_size_in_voxels) {
	if (edit.is_null()) {
		// ...
		return false;
	}

	Ref<SDFVoxEdit> vox_edit = edit;
	if (vox_edit.is_valid()) {
		String key = vox_edit->get_model_key();
		if (!_vox_model_cache.has(key)) {
			OS::get_singleton()->printerr("add_sdf_edit_at_world_pos: The model key '", key, "' has not been preloaded. Call preload_vox_model() first.");
			return false; // Abort placement
		}
		// Link the edit to the cached data.
		vox_edit->set_shared_data(_vox_model_cache[key]);
	}

	std::pair<Vector3, Vector3> bounds = edit->getApproximateWorldBounds();
	Vector3 worldMin = bounds.first;
	Vector3 worldMax = bounds.second;

	float paddingWorldUnits = 2.0f * voxel_scale;
	worldMin -= Vector3(paddingWorldUnits, paddingWorldUnits, paddingWorldUnits);
	worldMax += Vector3(paddingWorldUnits, paddingWorldUnits, paddingWorldUnits);

	const int inner_chunk_size = CS;
	float chunkWorldSize = static_cast<float>(inner_chunk_size) * voxel_scale;

	Vector3 minChunkCoords = Vector3(floor(worldMin.x / chunkWorldSize), floor(worldMin.y / chunkWorldSize), floor(worldMin.z / chunkWorldSize));
	Vector3 maxChunkCoords = Vector3(floor(worldMax.x / chunkWorldSize), floor(worldMax.y / chunkWorldSize), floor(worldMax.z / chunkWorldSize));

	for (int cx = static_cast<int>(minChunkCoords.x); cx <= static_cast<int>(maxChunkCoords.x); ++cx) {
		for (int cy = static_cast<int>(minChunkCoords.y); cy <= static_cast<int>(maxChunkCoords.y); ++cy) {
			for (int cz = static_cast<int>(minChunkCoords.z); cz <= static_cast<int>(maxChunkCoords.z); ++cz) {
				Vector3 currentChunkCoords(cx, cy, cz);
				if (!chunk_map.has(currentChunkCoords)) {
					add_chunk(currentChunkCoords, false);
				}

				ChunkMetadata *md = chunk_map.getptr(currentChunkCoords);
				if (md) {
					// BUGFIX: Always add new edits to the UnprocessedEdits list.
					// This ensures the update path for existing chunks receives the new edit.
					md->UnprocessedEdits.push_back(edit->clone());

					if (!md->is_dirty) {
						md->is_dirty = true;
						dirty_chunks.push_back(currentChunkCoords);
					}
				}
			}
		}
	}
	return true;
}

// Update recreate_chunk_mesh to use the pool.
// Update recreate_chunk_mesh to use the pool.
void VoxdotTerrain::recreate_chunk_mesh(const Vector3 &chunk_coords, int chunk_size_in_voxels) {
	ChunkMetadata *md = chunk_map.getptr(chunk_coords);
	if (!md) {
		OS::get_singleton()->printerr("recreate_chunk_mesh: No metadata found for chunk %s.\n", chunk_coords);
		return;
	}

	// If an old mesh instance ID exists, return it to the pool instead of deleting.
	if (md->meshInstance.is_valid()) {
		Object *obj = ObjectDB::get_instance(md->meshInstance);
		if (obj) {
			MeshInstance3D *old_mesh_instance = Object::cast_to<MeshInstance3D>(obj);
			if (old_mesh_instance) {
				return_mesh_instance_to_pool(old_mesh_instance);
			}
		}
	}
	md->meshInstance = ObjectID();

	// If an old collision shape ID exists, return it to the pool.
	if (md->collisionShape.is_valid()) {
		Object *obj = ObjectDB::get_instance(md->collisionShape);
		if (obj) {
			CollisionShape3D *old_collision_shape = Object::cast_to<CollisionShape3D>(obj);
			if (old_collision_shape) {
				return_collision_shape_to_pool(old_collision_shape);
			}
		}
	}
	md->collisionShape = ObjectID(); // Clear the old reference

	GodotMeshData mesh_data; // Declare it once

	generate_godot_mesh_for_chunk(mesh_data, chunk_coords, chunk_size_in_voxels);

	auto instance_time = std::chrono::high_resolution_clock::now();
	// Get a MeshInstance3D from the pool and use it.
	MeshInstance3D *mesh_instance = get_mesh_instance_from_pool();

	if (mesh_data.vertices.empty() || mesh_data.indices.empty()) {
		// If there is no mesh to display, return the instance to the pool
		return_mesh_instance_to_pool(mesh_instance);
		// Also ensure no collision shape is active for this chunk if mesh is empty
		md->collisionShape = ObjectID();
		return;
	}

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();

	Array mesh_arrays;
	mesh_arrays.resize(Mesh::ARRAY_MAX);

	PackedVector3Array vertices;
	for (size_t i = 0; i < mesh_data.vertices.size(); i += 3) {
		vertices.push_back(Vector3(mesh_data.vertices[i], mesh_data.vertices[i + 1], mesh_data.vertices[i + 2]));
	}
	mesh_arrays[Mesh::ARRAY_VERTEX] = vertices;

	// ... (rest of the mesh array setup: normals, uvs, colors, indices)

	if (!mesh_data.normals.empty()) {
		PackedVector3Array normals;
		for (size_t i = 0; i < mesh_data.normals.size(); i += 3) {
			normals.push_back(Vector3(
					mesh_data.normals[i],
					mesh_data.normals[i + 1],
					mesh_data.normals[i + 2]));
		}
		mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
	}

	if (!mesh_data.uvs.empty()) {
		PackedVector2Array uvs;
		for (size_t i = 0; i < mesh_data.uvs.size(); i += 2) {
			uvs.push_back(Vector2(
					mesh_data.uvs[i],
					mesh_data.uvs[i + 1]));
		}
		mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
	}
	if (!mesh_data.uvs2.empty()) {
		PackedVector2Array uvs2;
		for (size_t i = 0; i < mesh_data.uvs2.size(); i += 2) {
			uvs2.push_back(Vector2(
					mesh_data.uvs2[i],
					mesh_data.uvs2[i + 1]));
		}
		mesh_arrays[Mesh::ARRAY_TEX_UV2] = uvs2; // Assign to UV2 slot
	}
	if (!mesh_data.colors.empty()) {
		PackedColorArray colors;
		for (size_t i = 0; i < mesh_data.colors.size(); i += 4) {
			colors.push_back(Color(
					mesh_data.colors[i],
					mesh_data.colors[i + 1],
					mesh_data.colors[i + 2],
					mesh_data.colors[i + 3]));
		}
		mesh_arrays[Mesh::ARRAY_COLOR] = colors;
	}

	PackedInt32Array indices;
	for (size_t i = 0; i < mesh_data.indices.size(); ++i) {
		indices.push_back(mesh_data.indices[i]);
	}
	mesh_arrays[Mesh::ARRAY_INDEX] = indices;

	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mesh_arrays);

	mesh_instance->set_mesh(array_mesh);
	mesh_instance->set_surface_override_material(0, shared_material);
	const float chunk_world_size = CS * voxel_scale;
	Transform3D transform;
	transform.origin = Vector3(
			chunk_coords.x * chunk_world_size,
			chunk_coords.y * chunk_world_size,
			chunk_coords.z * chunk_world_size);
	mesh_instance->set_transform(transform);

	md->meshInstance = mesh_instance->get_instance_id();

	std::chrono::duration<double, std::micro> collision_duration_micro;

	// This single line replaces all the manual creation:
	auto collision_start = std::chrono::high_resolution_clock::now();

	CollisionShape3D *new_collision_shape = create_and_add_collision_shape_child(mesh_data, chunk_coords, voxel_scale);
	if (new_collision_shape) {
		md->collisionShape = new_collision_shape->get_instance_id();
	} else {
		md->collisionShape = ObjectID(); // Ensure it's cleared if creation fails
	}

	auto collision_end = std::chrono::high_resolution_clock::now();
	collision_duration_micro = collision_end - collision_start;
	std::cout << "collision execution time: " << collision_duration_micro.count() << " us" << "\n";

	auto instance_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::micro> instance_duration_micro = (instance_end - instance_time) - collision_duration_micro;
	std::cout << "Instance execution time: " << instance_duration_micro.count() << " us" << "\n";
}






// ----------------------------------------------------------------------------
// Less Important things
// ----------------------------------------------------------------------------

void VoxdotTerrain::set_voxel_scale(float p_scale) {
	voxel_scale = p_scale;
}

float VoxdotTerrain::get_voxel_scale() const {
	return voxel_scale;
}

void VoxdotTerrain::set_shared_material(Ref<Material> p_material) {
	shared_material = p_material;
}

Ref<Material> VoxdotTerrain::get_shared_material() const {
	return shared_material;
}

void VoxdotTerrain::set_noise(Ref<FastNoiseLite> p_noise) {
	noise = p_noise;
}

Ref<FastNoiseLite> VoxdotTerrain::get_noise() const {
	return noise;
}

// Implementations for noise_max and noise_base
void VoxdotTerrain::set_noise_max(float p_max) {
	noise_max = p_max;
}

float VoxdotTerrain::get_noise_max() const {
	return noise_max;
}

void VoxdotTerrain::set_noise_base(float p_base) {
	noise_base = p_base;
}

float VoxdotTerrain::get_noise_base() const {
	return noise_base;
}


//// Implementation for set_biomes and get_biomes
//void VoxdotTerrain::set_biomes(const Array &p_biomes) {
//	biomes.clear();
//	for (int i = 0; i < p_biomes.size(); ++i) {
//		Variant v = p_biomes[i];
//
//		// 1) What kind of Variant is this?
//		Variant::Type t = v.get_type();
//		String tname = Variant::get_type_name(t);
//		// 2) If it’s an Object, what’s its class?
//		Object *obj = (t == Variant::OBJECT ? v : nullptr);
//		String cname = obj ? obj->get_class() : "<no‑object>";
//
//		OS::get_singleton()->printerr(
//				"set_biomes[%d]: Variant type=%s, class=%s",
//						i, tname, cname);
//
//		// Now try to cast
//		Ref<Biome> b;
//		if (obj) {
//			b = Object::cast_to<Biome>(obj);
//		}
//		if (b.is_valid()) {
//			biomes.push_back(b);
//		} else {
//			OS::get_singleton()->printerr(" → FAILED to cast to Biome at index %d", i);
//		}
//	}
//}
//
//Array VoxdotTerrain::get_biomes() const {
//	Array biomes_array;
//	for (int i = 0; i < biomes.size(); ++i) {
//		biomes_array.push_back(biomes[i]);
//	}
//	return biomes_array;
//}

void VoxdotTerrain::set_biomes(const Array &p_biomes) {
	biomes.clear();
	for (int i = 0; i < p_biomes.size(); ++i) {
		Variant v = p_biomes[i];
		if (v.get_type() == Variant::OBJECT && Object::cast_to<Biome>(v)) {
			biomes.push_back(v);
		} else {
			// user clicked + but didn’t assign: give them a default
			biomes.push_back(memnew(Biome));
		}
	}
}

Array VoxdotTerrain::get_biomes() const {
	Array out;
	for (auto &bm : biomes) {
		out.append(bm);
	}
	return out;
}

// Implement the getter and setter for the entire array
// Implement the setter (converts Godot Array to internal Godot Vector)
void VoxdotTerrain::set_voxel_type_properties_array(const Array &p_array) {
	// Resize internal vector if necessary, but keep it at 256
	int size_to_copy = MIN(p_array.size(), 256);
	_voxel_type_properties.resize(256); // Always maintain 256 elements

	for (int i = 0; i < 256; ++i) {
		if (i < size_to_copy) {
			// Try to cast the Variant to Ref<VoxelMaterialProperties>
			Ref<VoxelMaterialProperties> props = p_array[i];
			if (props.is_valid()) {
				_voxel_type_properties.set(i, props);
			} else {
				// If invalid or wrong type, instantiate a default one
				Ref<VoxelMaterialProperties> default_props;
				default_props.instantiate();
				_voxel_type_properties.set(i, default_props);
			}
		} else {
			// For elements beyond the size of p_array, ensure they are instantiated
			if (!_voxel_type_properties[i].is_valid()) {
				Ref<VoxelMaterialProperties> default_props;
				default_props.instantiate();
				_voxel_type_properties.set(i, default_props);
			}
		}
	}
	// You might want to trigger a mesh regeneration if properties change
}

// Implement the getter (converts internal Godot Vector to Godot Array)
Array VoxdotTerrain::get_voxel_type_properties_array() const {
	Array godot_array;
	godot_array.resize(_voxel_type_properties.size()); // Size matches the internal Vector (256)
	for (int i = 0; i < _voxel_type_properties.size(); ++i) {
		godot_array[i] = _voxel_type_properties[i]; // Assign Ref<T> directly
	}
	return godot_array;
}

void VoxdotTerrain::set_global_structures(const Array &structures) {
	global_structures.clear();
	for (int i = 0; i < structures.size(); ++i) {
		Variant v = structures[i];
		Ref<VoxWorldStructure> tl = v;
		if (tl.is_valid()) {
			// user assigned a resource
			global_structures.push_back(tl);
		} else {
			// auto‑instantiate a fresh TerrainLayer
			Ref<VoxWorldStructure> new_tl = memnew(VoxWorldStructure);
			global_structures.push_back(new_tl);
		}
	}
}

Array VoxdotTerrain::get_global_structures() const {
	Array out;
	for (auto &tl : global_structures) {
		out.append(tl);
	}
	return out;
}

static uint32_t hash_coords_and_seed(const Vector3 &p_coords, int p_seed) {
	uint32_t x = static_cast<uint32_t>(p_coords.x);
	uint32_t y = static_cast<uint32_t>(p_coords.y);
	uint32_t z = static_cast<uint32_t>(p_coords.z);
	uint32_t seed = static_cast<uint32_t>(p_seed);

	uint32_t h = seed;
	h = ((h ^ x) * 0x85ebca6b);
	h = ((h ^ y) * 0xc2b2ae35);
	h = ((h ^ z) * 0x27d4eb2d);
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

void VoxdotTerrain::process_structures_for_chunk(const Vector3 &chunk_coords) {
	ChunkMetadata *md = chunk_map.getptr(chunk_coords);
	if (!md) {
		return;
	}

	// --- 1. Gather all applicable structures ---
	Vector<Ref<VoxWorldStructure>> structures_to_check;

	// Add global structures
	for (int i = 0; i < global_structures.size(); ++i) {
		Ref<VoxWorldStructure> structure = global_structures[i];
		if (structure.is_valid()) {
			structures_to_check.push_back(structure);
		}
	}

	// Determine biome and add biome-specific structures
	const int inner_chunk_size = CS;
	const Vector3 chunk_center_world_pos = (chunk_coords * inner_chunk_size + Vector3(inner_chunk_size / 2.0f, inner_chunk_size / 2.0f, inner_chunk_size / 2.0f)) * voxel_scale;

	if (!biomes.is_empty()) {
		const float biome_freq_multiplier = 0.05f; // Same as in populate_chunk_voxels
		const float biome_selection_noise = noise->get_noise_2d(chunk_center_world_pos.x * biome_freq_multiplier, chunk_center_world_pos.z * biome_freq_multiplier);
		int biome_index = floor((biome_selection_noise * 0.5f + 0.5f) * biomes.size());
		biome_index = CLAMP(biome_index, 0, biomes.size() - 1);
		Ref<Biome> biome = biomes[biome_index];

		if (biome.is_valid()) {
			Array biome_structures = biome->get_structures();
			for (int i = 0; i < biome_structures.size(); ++i) {
				Ref<VoxWorldStructure> structure = biome_structures[i];
				if (structure.is_valid()) {
					structures_to_check.push_back(structure);
				}
			}
		}
	}

	// --- 2. Iterate through structures and check for placement ---
	for (const Ref<VoxWorldStructure> &structure : structures_to_check) {
		Vector3 spawn_pos;
		bool should_attempt_spawn = false;

		if (structure->get_spawn_type() == VoxWorldStructure::SPAWN_ONCE) {
			const float chunk_world_size = inner_chunk_size * voxel_scale;
			if (chunk_world_size <= 0.0f) {
				continue;
			}
			Vector3 target_chunk_coords = (structure->get_spawn_position() / chunk_world_size).floor();
			if (target_chunk_coords == chunk_coords) {
				spawn_pos = structure->get_spawn_position();
				should_attempt_spawn = true;
			}
		} else { // SPAWN_RANDOM
			uint32_t hash_val = hash_coords_and_seed(chunk_coords, structure->get_random_seed());
			float random_chance = (hash_val & 0xFFFFFF) / 16777215.0f; // Use 24 bits for a float 0-1

			if (random_chance < structure->get_spawn_frequency()) {
				uint32_t pos_hash = hash_coords_and_seed(chunk_coords, structure->get_random_seed() + 1);
				float off_x = ((pos_hash >> 0) & 0xFF) / 255.0f * inner_chunk_size * voxel_scale;
				float off_y = ((pos_hash >> 8) & 0xFF) / 255.0f * inner_chunk_size * voxel_scale;
				float off_z = ((pos_hash >> 16) & 0xFF) / 255.0f * inner_chunk_size * voxel_scale;

				spawn_pos = (chunk_coords * inner_chunk_size * voxel_scale) + Vector3(off_x, off_y, off_z);
				should_attempt_spawn = true;
			}
		}

		if (should_attempt_spawn) {
			// --- 3. Finalize spawn position with alignment and offset ---
			if (structure->get_ground_aligned()) {
				spawn_pos.y = get_surface_height_at(spawn_pos.x, spawn_pos.z);
			}
			spawn_pos += structure->get_position_offset();

			// --- 4. Perform spawn condition checks ---
			if (spawn_pos.y < structure->get_min_altitude() || spawn_pos.y > structure->get_max_altitude()) {
				continue;
			}

			Ref<FastNoiseLite> noise_check = structure->get_noise_layer();
			if (noise_check.is_valid()) {
				float noise_val = noise_check->get_noise_3d(spawn_pos.x, spawn_pos.y, spawn_pos.z);
				if (noise_val < structure->get_required_noise_value_min() || noise_val > structure->get_required_noise_value_max()) {
					continue;
				}
			}

			// --- 5. All checks passed, create and add the edit ---
			Ref<SDFVoxEdit> vox_edit;
			vox_edit.instantiate();
			vox_edit->set_offset(spawn_pos);
			vox_edit->set_material(0); // 0 = Use material from .vox file
			vox_edit->set_model_key(structure->get_model_key());
			vox_edit->set_scale(voxel_scale);

			add_sdf_edit_at_world_pos(vox_edit, CS + 2);
		}
	}
}

float VoxdotTerrain::get_surface_height_at(float world_x, float world_z) const {
	if (biomes.is_empty()) {
		return 0.f; // Cannot determine height without biomes/layers
	}

	// Determine Biome at the given world coordinates
	const float biome_freq_multiplier = 0.05f;
	const float biome_selection_noise = noise->get_noise_2d(world_x * biome_freq_multiplier, world_z * biome_freq_multiplier);
	int biome_index = floor((biome_selection_noise * 0.5f + 0.5f) * biomes.size());
	biome_index = CLAMP(biome_index, 0, biomes.size() - 1);
	const Ref<Biome> biome = biomes[biome_index];

	if (biome.is_null() || biome->get_terrain_layers().is_empty()) {
		return 0.0f;
	}

	const Array &layers = biome->get_terrain_layers();

	// To properly find the surface, we must check layers from top to bottom, just like in populate_chunk_voxels.
	// The code iterates layers backwards (which is assumed to be top-down).
	for (int i = layers.size() - 1; i >= 0; --i) {
		Ref<TerrainLayer> layer = layers[i];
		if (layer.is_valid()) {
			// We only care about 2D layers for a definitive surface height.
			if (!layer->get_dimension()) {
				const float noise_base = layer->get_noise_base();
				const float noise_scale = layer->get_noise_max() * 2.0f;
				const float noise_val = layer->get_noise()->get_noise_2d(world_x, world_z);
				// This height is assumed to be in world units, matching the terrain generation logic.
				const float height = noise_base + (noise_val * noise_scale);

				// Assuming this layer is opaque and replaces whatever is below, this is our surface height.
				return height;
			}
			// If the top layer is 3D (e.g., floating islands), ground alignment is ambiguous.
			// We will continue searching downwards for the first 2D layer to act as the "ground".
		}
	}

	return 0.0f; // No 2D layers found to align to.
}





void VoxdotTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("import_palette_png", "path"), &VoxdotTerrain::import_palette_png);

	// Terrain system initialization/cleanup
	ClassDB::bind_method(D_METHOD("init_terrain_system", "initial_voxel_scale", "pool_size"), &VoxdotTerrain::init_terrain_system, DEFVAL(0.1f), DEFVAL(500));
	ClassDB::bind_method(D_METHOD("cleanup_terrain_system"), &VoxdotTerrain::cleanup_terrain_system);

	// Chunk management
	ClassDB::bind_method(D_METHOD("add_chunk", "coords"), &VoxdotTerrain::add_chunk);
	ClassDB::bind_method(D_METHOD("remove_chunk", "coords"), &VoxdotTerrain::remove_chunk);
	ClassDB::bind_method(D_METHOD("has_chunk", "coords"), &VoxdotTerrain::has_chunk);
	ClassDB::bind_method(D_METHOD("clear_all_chunks"), &VoxdotTerrain::clear_all_chunks);
	ClassDB::bind_method(D_METHOD("get_loaded_chunk_coords"), &VoxdotTerrain::get_loaded_chunk_coords);
	ClassDB::bind_method(D_METHOD("process_dirty_chunks", "max_chunks_to_process", "FirstInFirstOut"), &VoxdotTerrain::process_dirty_chunks);
	ClassDB::bind_method(D_METHOD("recreate_chunk_mesh", "chunk_coords", "chunk_size_in_voxels"), &VoxdotTerrain::recreate_chunk_mesh);
	ClassDB::bind_method(D_METHOD("is_chunk_partially_filled", "chunk_coords", "check_padding"), &VoxdotTerrain::is_chunk_partially_filled);

	// SDF edit application
	ClassDB::bind_method(D_METHOD("place_edit", "size", "world_pos", "material", "shape"), &VoxdotTerrain::add_edit_wrapper);
	ClassDB::bind_method(D_METHOD("place_vox_edit", "key", "world_pos", "material"), &VoxdotTerrain::add_vox_edit_wrapper);
	ClassDB::bind_method(D_METHOD("preload_vox_model", "key", "path"), &VoxdotTerrain::preload_vox_model);

	// Voxel scale accessors
	ClassDB::bind_method(D_METHOD("set_voxel_scale", "scale"), &VoxdotTerrain::set_voxel_scale);
	ClassDB::bind_method(D_METHOD("get_voxel_scale"), &VoxdotTerrain::get_voxel_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "voxel_scale"), "set_voxel_scale", "get_voxel_scale");

	// Shared Material accessors
	ClassDB::bind_method(D_METHOD("set_shared_material", "material"), &VoxdotTerrain::set_shared_material);
	ClassDB::bind_method(D_METHOD("get_shared_material"), &VoxdotTerrain::get_shared_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shared_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_shared_material", "get_shared_material");

	// Noise resource accessors
	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxdotTerrain::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxdotTerrain::get_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise", "get_noise");

	ClassDB::bind_method(D_METHOD("set_noise_base", "base"), &VoxdotTerrain::set_noise_base);
	ClassDB::bind_method(D_METHOD("get_noise_base"), &VoxdotTerrain::get_noise_base);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_base"), "set_noise_base", "get_noise_base");

	ClassDB::bind_method(D_METHOD("set_noise_max", "max"), &VoxdotTerrain::set_noise_max);
	ClassDB::bind_method(D_METHOD("get_noise_max"), &VoxdotTerrain::get_noise_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_max"), "set_noise_max", "get_noise_max");

	ClassDB::bind_method(D_METHOD("get_surface_height_at", "world_x", "world_z"), &VoxdotTerrain::get_surface_height_at);

	// NEW: World (Biome array) accessors
	ClassDB::bind_method(D_METHOD("set_biomes", "v"), &VoxdotTerrain::set_biomes);
	ClassDB::bind_method(D_METHOD("get_biomes"), &VoxdotTerrain::get_biomes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "biomes", PROPERTY_HINT_ARRAY_TYPE, "Biome"), "set_biomes", "get_biomes");

	ClassDB::bind_method(D_METHOD("set_voxel_type_properties_array", "properties"), &VoxdotTerrain::set_voxel_type_properties_array);
	ClassDB::bind_method(D_METHOD("get_voxel_type_properties_array"), &VoxdotTerrain::get_voxel_type_properties_array);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "voxel_type_properties_array", PROPERTY_HINT_ARRAY_TYPE, "VoxelMaterialProperties"), "set_voxel_type_properties_array", "get_voxel_type_properties_array");

	ClassDB::bind_method(D_METHOD("set_global_structures", "structures"), &VoxdotTerrain::set_global_structures);
	ClassDB::bind_method(D_METHOD("get_global_structures"), &VoxdotTerrain::get_global_structures);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "global_structures", PROPERTY_HINT_ARRAY_TYPE, "VoxWorldStructure"), "set_global_structures", "get_global_structures");



	
}
