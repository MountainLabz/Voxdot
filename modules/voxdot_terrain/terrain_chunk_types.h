#ifndef TERRAIN_CHUNK_TYPES_H
#define TERRAIN_CHUNK_TYPES_H

#include "core/math/math_funcs.h" // Still useful for other Math functions like abs, floor, ceil
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include <algorithm> // For std::min and std::max
#include "ogt_vox.h" // Include ogt_vox.h here as SDFVoxEdit will use it
#include <string> // For std::string in SDFVoxEdit
#include <vector> // For std::vector in SDFVoxEdit (to store voxel data from .vox)
#include <fstream> // For file reading in SDFVoxEdit
#include <limits> // For numeric_limits in SDFVoxEdit


// Hash and equality for Godot's Vector3 to use as a HashMap key
struct Vector3Hasher {
	// Using a custom hash function based on your suggestion,
	// as Godot's Vector3::hash() and Math::hash_murmur3_one_32 seem unavailable in your setup.
	static _FORCE_INLINE_ uint32_t hash(const Vector3 &p_v) {
		// Convert float components to int for hashing if directly using the formula,
		// or use them as-is depending on the desired hash distribution.
		// For consistency with integer voxel coordinates, casting to int is reasonable.
		uint64_t x = static_cast<uint64_t>(p_v.x);
		uint64_t y = static_cast<uint64_t>(p_v.y);
		uint64_t z = static_cast<uint64_t>(p_v.z);
		uint64_t h = (x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u);
		return static_cast<uint32_t>(h); // Cast to uint32_t for Godot's hash return type
	}
};

struct Vector3Equals {
	static _FORCE_INLINE_ bool compare(const Vector3 &p_a, const Vector3 &p_b) {
		return p_a == p_b;
	}
};

// ----------------------------------------------------------------------------
// Base SDF Edit Interface (Godot-ified)
// ----------------------------------------------------------------------------
class ISDFEdit : public RefCounted {
	GDCLASS(ISDFEdit, RefCounted);

public:
	virtual float getSignedDistance(const Vector3 &point) const = 0;
	virtual uint8_t getMaterial(const Vector3 &p_pos) const = 0;
	virtual std::pair<Vector3, Vector3> getApproximateWorldBounds() const = 0;
	virtual Ref<ISDFEdit> clone() const = 0;
	virtual ~ISDFEdit() = default;

protected:
	static void _bind_methods();
};

// ----------------------------------------------------------------------------
// SDF Sphere Edit Structure (Godot-ified)
// ----------------------------------------------------------------------------
class SDFSphereEdit : public ISDFEdit {
	GDCLASS(SDFSphereEdit, ISDFEdit);

public:
	Vector3 center;
	float radius;
	uint8_t material;

	SDFSphereEdit() :
			center(Vector3()), radius(0.0f), material(0) {}
	SDFSphereEdit(const Vector3 &c, float r, uint8_t m) :
			center(c), radius(r), material(m) {}

	float getSignedDistance(const Vector3 &point) const override {
		return point.distance_to(center) - radius;
	}
	uint8_t getMaterial(const Vector3 &p_pos) const override { return material; }
	std::pair<Vector3, Vector3> getApproximateWorldBounds() const override {
		Vector3 worldMin = center - Vector3(radius, radius, radius);
		Vector3 worldMax = center + Vector3(radius, radius, radius);
		return { worldMin, worldMax };
	}
	Ref<ISDFEdit> clone() const override {
		Ref<SDFSphereEdit> new_clone;
		new_clone.instantiate();
		new_clone->center = center;
		new_clone->radius = radius;
		new_clone->material = material;
		return new_clone;
	}

protected:
	static void _bind_methods();

private:
	void set_center(const Vector3 &p_center) { center = p_center; }
	Vector3 get_center() const { return center; }
	void set_radius(float p_radius) { radius = p_radius; }
	float get_radius() const { return radius; }
	void set_material(uint8_t p_material) { material = p_material; }
	uint8_t get_material() const { return material; }
};

// ----------------------------------------------------------------------------
// SDF Cube Edit Structure (Godot-ified)
// ----------------------------------------------------------------------------
class SDFCubeEdit : public ISDFEdit {
	GDCLASS(SDFCubeEdit, ISDFEdit);

public:
	Vector3 center;
	Vector3 halfExtents;
	uint8_t material;

	SDFCubeEdit() :
			center(Vector3()), halfExtents(Vector3()), material(0) {}
	SDFCubeEdit(const Vector3 &c, const Vector3 &he, uint8_t m) :
			center(c), halfExtents(he), material(m) {}

	float getSignedDistance(const Vector3 &p) const override {
		// Vector3::abs() is a member function.
		Vector3 q = (p - center).abs() - halfExtents;

		// Using std::max and std::min from <algorithm> as a workaround.
		// These are standard C++ functions and should always be available.
		float inner_max = std::max(q.x, std::max(q.y, q.z));
		return q.max(Vector3(0.0f, 0.0f, 0.0f)).length() + std::min(inner_max, 0.0f);
	}
	uint8_t getMaterial(const Vector3 &p_pos) const override { return material; }
	std::pair<Vector3, Vector3> getApproximateWorldBounds() const override {
		Vector3 worldMin = center - halfExtents;
		Vector3 worldMax = center + halfExtents;
		return { worldMin, worldMax };
	}
	Ref<ISDFEdit> clone() const override {
		Ref<SDFCubeEdit> new_clone;
		new_clone.instantiate();
		new_clone->center = center;
		new_clone->halfExtents = halfExtents;
		new_clone->material = material;
		return new_clone;
	}

protected:
	static void _bind_methods();

private:
	void set_center(const Vector3 &p_center) { center = p_center; }
	Vector3 get_center() const { return center; }
	void set_half_extents(const Vector3 &p_half_extents) { halfExtents = p_half_extents; }
	Vector3 get_half_extents() const { return halfExtents; }
	void set_material(uint8_t p_material) { material = p_material; }
	uint8_t get_material() const { return material; }
};

// ----------------------------------------------------------------------------
// SDF Vox Edit
// ----------------------------------------------------------------------------
class SDFVoxEdit : public ISDFEdit {
	GDCLASS(SDFVoxEdit, ISDFEdit);

private:
	String _file_path;
	Vector3 _offset; // Offset for the voxel model in world units
	uint8_t _material;
	float _scale = 1.0f; // NEW: Scale factor for the voxel model, default to 1.0

	// Cached .vox scene data
	mutable ogt_vox_scene *_vox_scene_cache = nullptr;
	mutable PackedByteArray _file_buffer_cache; // Godot's PackedByteArray for file content

	// Cached overall AABB of the voxel scene in its local space (relative to _offset)
	mutable Vector3 _cached_scene_aabb_min_local;
	mutable Vector3 _cached_scene_aabb_max_local;
	mutable bool _are_bounds_cached = false;

	// NEW: Members for pre-processed voxel data
	mutable std::vector<uint8_t> _preprocessed_voxels;
	mutable Vector3 _model_dimensions = Vector3(0, 0, 0); // Dimensions of the pre-processed voxel grid (x, y, z)
	mutable Vector3 _model_min_corner = Vector3(0, 0, 0); // Local space minimum corner of the voxel model
	mutable bool _is_preprocessed = false;

	void _load_vox_file() const {
		if (_vox_scene_cache && !_file_buffer_cache.is_empty()) {
			return; // Already loaded and valid
		}

		// Clear existing data before attempting to load new file
		_clear_vox_cache();

		Ref<FileAccess> file = FileAccess::open(_file_path, FileAccess::READ);
		if (file.is_null()) {
			OS::get_singleton()->printerr("SDFVoxEdit: Failed to open .vox file: %s", _file_path);
			return;
		}

		_file_buffer_cache.resize(file->get_length());
		file->get_buffer(_file_buffer_cache.ptrw(), file->get_length());
		file->close();

		_vox_scene_cache = const_cast<ogt_vox_scene *>(ogt_vox_read_scene(_file_buffer_cache.ptr(), _file_buffer_cache.size()));
		if (!_vox_scene_cache) {
			OS::get_singleton()->printerr("SDFVoxEdit: Failed to read .vox scene from file: %s", _file_path);
			_file_buffer_cache.clear(); // Clear buffer if loading failed
		}
	}

	void _clear_vox_cache() const {
		if (_vox_scene_cache) {
			ogt_vox_destroy_scene(_vox_scene_cache);
			_vox_scene_cache = nullptr;
		}
		_file_buffer_cache.clear();
		_are_bounds_cached = false; // Invalidate bounds cache
		_is_preprocessed = false; // Invalidate preprocessed cache
		_preprocessed_voxels.clear();
		_model_dimensions = Vector3(0, 0, 0);
		_model_min_corner = Vector3(0, 0, 0);
	}

	void _calculate_and_cache_local_bounds() const { // New helper to compute/cache AABB
		if (_are_bounds_cached && _vox_scene_cache) { // Only recalculate if path changed or not loaded
			return;
		}
		_load_vox_file(); // Ensure scene is loaded

		if (!_vox_scene_cache || _vox_scene_cache->num_models == 0) {
			_cached_scene_aabb_min_local = Vector3(0, 0, 0);
			_cached_scene_aabb_max_local = Vector3(0, 0, 0);
			_are_bounds_cached = true;
			return;
		}

		Vector3 min_bounds_local = Vector3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		Vector3 max_bounds_local = Vector3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

		for (uint32_t i = 0; i < _vox_scene_cache->num_instances; ++i) {
			const ogt_vox_instance *instance = &_vox_scene_cache->instances[i];
			const ogt_vox_model *model = _vox_scene_cache->models[instance->model_index];

			if (!model) {
				continue;
			}

			// Model dimensions from ogt_vox (MagicaVoxel's internal X, Y, Z)
			Vector3 model_size_magicavoxel = Vector3(model->size_x, model->size_y, model->size_z);

			// Instance translation (corner of the model in MagicaVoxel's coordinate system)
			Vector3 instance_translation_magicavoxel = Vector3(instance->transform.m30, instance->transform.m31, instance->transform.m32);

			// Convert to Godot's coordinate system: X->X, Y_magica->Z_godot, Z_magica->Y_godot
			Vector3 transformed_instance_translation_godot_space = Vector3(
					instance_translation_magicavoxel.x,
					instance_translation_magicavoxel.z, // MagicaVoxel Z is Godot Y (height)
					instance_translation_magicavoxel.y // MagicaVoxel Y is Godot Z (depth)
			);
			Vector3 transformed_model_size_godot_space = Vector3(
					model_size_magicavoxel.x,
					model_size_magicavoxel.z, // MagicaVoxel Z is Godot Y (height)
					model_size_magicavoxel.y // MagicaVoxel Y is Godot Z (depth)
			);

			// Apply the SDFVoxEdit's scale to the dimensions
			Vector3 current_min_instance = transformed_instance_translation_godot_space * _scale;
			Vector3 current_max_instance = (transformed_instance_translation_godot_space + transformed_model_size_godot_space) * _scale;

			min_bounds_local.x = MIN(min_bounds_local.x, current_min_instance.x);
			min_bounds_local.y = MIN(min_bounds_local.y, current_min_instance.y);
			min_bounds_local.z = MIN(min_bounds_local.z, current_min_instance.z);

			max_bounds_local.x = MAX(max_bounds_local.x, current_max_instance.x);
			max_bounds_local.y = MAX(max_bounds_local.y, current_max_instance.y);
			max_bounds_local.z = MAX(max_bounds_local.z, current_max_instance.z);
		}
		_cached_scene_aabb_min_local = min_bounds_local;
		_cached_scene_aabb_max_local = max_bounds_local;
		_are_bounds_cached = true;
	}

	float _calculate_sdf_to_box(const Vector3 &point, const Vector3 &box_min, const Vector3 &box_max) const {
		// Based on https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm for AABB
		Vector3 p_clamped = point.clamp(box_min, box_max);
		float d_outside = point.distance_to(p_clamped);
		float d_inside = 0.0f;

		if (point == p_clamped) { // Point is inside the AABB
			d_inside = MAX(MAX(box_min.x - point.x, point.x - box_max.x),
					MAX(box_min.y - point.y, point.y - box_max.y));
			d_inside = MAX(d_inside, MAX(box_min.z - point.z, point.z - box_max.z));
		}
		return d_outside - d_inside; // Positive outside, negative inside
	}

	void _preprocess_vox_data() const {
		if (_is_preprocessed && _vox_scene_cache) {
			return; // Already preprocessed
		}

		_load_vox_file(); // Ensure scene is loaded

		if (!_vox_scene_cache || _vox_scene_cache->num_models == 0) {
			_preprocessed_voxels.clear();
			_model_dimensions = Vector3(0, 0, 0);
			_model_min_corner = Vector3(0, 0, 0);
			_is_preprocessed = true;
			return;
		}

		// Calculate overall dimensions and min corner of the combined voxel data
		// Initialize with extreme values to find true min/max
		Vector3 min_voxel_coord = Vector3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		Vector3 max_voxel_coord = Vector3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

		for (uint32_t i = 0; i < _vox_scene_cache->num_instances; ++i) {
			const ogt_vox_instance *instance = &_vox_scene_cache->instances[i];
			const ogt_vox_model *model = _vox_scene_cache->models[instance->model_index];
			if (!model) {
				continue;
			}

			// Convert MagicaVoxel instance translation to Godot (X->X, Y->Z, Z->Y)
			Vector3 instance_translation_godot = Vector3(instance->transform.m30, instance->transform.m32, instance->transform.m31);
			Vector3 model_size_godot = Vector3(model->size_x, model->size_z, model->size_y); // MagicaVoxel Z is Godot Y

			// For now, assuming no rotation from instance->transform.
			// If rotations are used in .vox, this section needs proper matrix transformation
			// to get the correct transformed min/max for each model instance.
			// As instance.transform is a 4x3 matrix from ogt_vox, if it contains rotation,
			// you'd typically need to apply it to the model's corners.
			// For simplicity with just translation in MagicaVoxel, it's min_pos and min_pos + size.

			min_voxel_coord.x = MIN(min_voxel_coord.x, instance_translation_godot.x);
			min_voxel_coord.y = MIN(min_voxel_coord.y, instance_translation_godot.y);
			min_voxel_coord.z = MIN(min_voxel_coord.z, instance_translation_godot.z);

			max_voxel_coord.x = MAX(max_voxel_coord.x, instance_translation_godot.x + model_size_godot.x - 1); // -1 for max inclusive voxel coord
			max_voxel_coord.y = MAX(max_voxel_coord.y, instance_translation_godot.y + model_size_godot.y - 1);
			max_voxel_coord.z = MAX(max_voxel_coord.z, instance_translation_godot.z + model_size_godot.z - 1);
		}

		// Determine overall dimensions for the dense voxel grid
		_model_min_corner = Vector3(Math::floor(min_voxel_coord.x), Math::floor(min_voxel_coord.y), Math::floor(min_voxel_coord.z));
		_model_dimensions = Vector3(Math::ceil(max_voxel_coord.x) + 1, Math::ceil(max_voxel_coord.y) + 1, Math::ceil(max_voxel_coord.z) + 1) - _model_min_corner;

		// Ensure dimensions are at least 1 if a model exists
		if (_model_dimensions.x < 1 && _vox_scene_cache->num_models > 0) {
			_model_dimensions.x = 1;
		}
		if (_model_dimensions.y < 1 && _vox_scene_cache->num_models > 0) {
			_model_dimensions.y = 1;
		}
		if (_model_dimensions.z < 1 && _vox_scene_cache->num_models > 0) {
			_model_dimensions.z = 1;
		}

		// Resize the preprocessed voxel array
		size_t total_voxels = static_cast<size_t>(_model_dimensions.x) * static_cast<size_t>(_model_dimensions.y) * static_cast<size_t>(_model_dimensions.z);
		_preprocessed_voxels.assign(total_voxels, 0); // Initialize with 0 (empty)

		// Populate the preprocessed voxel array
		for (uint32_t i = 0; i < _vox_scene_cache->num_instances; ++i) {
			const ogt_vox_instance *instance = &_vox_scene_cache->instances[i];
			const ogt_vox_model *model = _vox_scene_cache->models[instance->model_index];
			if (!model) {
				continue;
			}

			Vector3 instance_translation_godot = Vector3(instance->transform.m30, instance->transform.m32, instance->transform.m31);

			for (uint32_t z_model = 0; z_model < model->size_z; ++z_model) { // Iterate over model's Z (MagicaVoxel)
				for (uint32_t y_model = 0; y_model < model->size_y; ++y_model) { // Iterate over model's Y (MagicaVoxel)
					for (uint32_t x_model = 0; x_model < model->size_x; ++x_model) { // Iterate over model's X (MagicaVoxel)
						uint8_t color_index = model->voxel_data[x_model + y_model * model->size_x + z_model * model->size_x * model->size_y];

						if (color_index != 0) { // If voxel is not empty (MagicaVoxel color index 0 is typically empty)
							// Voxel position in MagicaVoxel model space
							Vector3 voxel_pos_model_space_magicavoxel = Vector3(x_model, y_model, z_model);

							// Convert to Godot space for position relative to instance origin
							Vector3 voxel_pos_godot_relative_to_instance_origin = Vector3(
									voxel_pos_model_space_magicavoxel.x,
									voxel_pos_model_space_magicavoxel.z, // MagicaVoxel Z is Godot Y
									voxel_pos_model_space_magicavoxel.y // MagicaVoxel Y is Godot Z
							);

							// World voxel position relative to scene origin (before _model_min_corner translation)
							Vector3 world_voxel_pos = instance_translation_godot + voxel_pos_godot_relative_to_instance_origin;

							// Convert to local grid coordinates for _preprocessed_voxels
							Vector3 grid_pos = world_voxel_pos - _model_min_corner;

							int gx = static_cast<int>(Math::round(grid_pos.x));
							int gy = static_cast<int>(Math::round(grid_pos.y));
							int gz = static_cast<int>(Math::round(grid_pos.z));

							if (gx >= 0 && gx < _model_dimensions.x &&
									gy >= 0 && gy < _model_dimensions.y &&
									gz >= 0 && gz < _model_dimensions.z) {
								size_t index = gx + static_cast<size_t>(gy) * static_cast<size_t>(_model_dimensions.x) + static_cast<size_t>(gz) * static_cast<size_t>(_model_dimensions.x) * static_cast<size_t>(_model_dimensions.y);
								if (index < _preprocessed_voxels.size()) {
									// --- START OF ADDED/MODIFIED CODE ---
									uint8_t processed_material_id = color_index;
									// If the material ID is 0, remap it to 1 to ensure it's treated as a solid voxel.
									// This handles cases where a solid voxel from the .vox file might have a color_index of 0.
									if (processed_material_id == 0) {
										processed_material_id = 1;
									}
									_preprocessed_voxels[index] = processed_material_id; // Store the processed material/color index
									// --- END OF ADDED/MODIFIED CODE ---
								}
							}
						}
					}
				}
			}
		}
		_is_preprocessed = true;
	}

public:
	SDFVoxEdit() :
			_vox_scene_cache(nullptr),
			_are_bounds_cached(false),
			_is_preprocessed(false),
			_material(0),
			_scale(1.0f) {
		// _file_path, _offset, _model_dimensions, _model_min_corner are default-constructed
	}

	~SDFVoxEdit() {
		_clear_vox_cache();
	}

	void set_file_path(const String &p_path) {
		if (_file_path != p_path) {
			_file_path = p_path;
			_clear_vox_cache(); // Clear cache to force reload and re-preprocess
		}
	}
	String get_file_path() const {
		return _file_path;
	}

	void set_offset(const Vector3 &p_offset) {
		_offset = p_offset;
		// Offset change doesn't invalidate internal voxel data or bounds, only world position
		// so no need to clear _are_bounds_cached or _is_preprocessed
	}
	Vector3 get_offset() const {
		return _offset;
	}

	void set_material(uint8_t p_material) {
		_material = p_material;
	}
	uint8_t get_material() const {
		return _material;
	}

	void set_scale(float p_scale) {
		if (_scale != p_scale) {
			_scale = p_scale;
			_are_bounds_cached = false; // Invalidate bounds if scale changes
			_is_preprocessed = false; // Invalidate preprocessed cache if scale changes
		}
	}
	float get_scale() const {
		return _scale;
	}

	virtual float getSignedDistance(const Vector3 &p_pos) const override {
		if (!_is_preprocessed) {
			_preprocess_vox_data();
		}

		Vector3 local_pos = (p_pos - _offset) / _scale;

		// Convert local_pos to voxel coordinates relative to the preprocessed grid's (0,0,0)
		int vx = static_cast<int>(Math::floor(local_pos.x - _model_min_corner.x));
		int vy = static_cast<int>(Math::floor(local_pos.y - _model_min_corner.y));
		int vz = static_cast<int>(Math::floor(local_pos.z - _model_min_corner.z));

		// Explicitly cast _model_dimensions components to int for clamping and index calculation
		int dim_x_int = static_cast<int>(_model_dimensions.x);
		int dim_y_int = static_cast<int>(_model_dimensions.y);
		int dim_z_int = static_cast<int>(_model_dimensions.z);

		// Clamp voxel coordinates to be within the pre-processed model dimensions
		vx = std::clamp(vx, 0, dim_x_int - 1);
		vy = std::clamp(vy, 0, dim_y_int - 1);
		vz = std::clamp(vz, 0, dim_z_int - 1);

		size_t index = static_cast<size_t>(vx + vy * dim_x_int + vz * dim_x_int * dim_y_int);

		if (index < _preprocessed_voxels.size()) {
			return _preprocessed_voxels[index] != 0 ? -1.0f : 1.0f; // -1.0f for solid, 1.0f for air
		}
		return 1.0f; // Outside bounds or invalid index implies air
	}

	virtual uint8_t getMaterial(const Vector3 &p_pos) const override {
		if (!_is_preprocessed) {
			_preprocess_vox_data(); // This will trigger preprocessing
		}

		// Convert world position to local voxel grid coordinates
		Vector3 local_pos = (p_pos - _offset) / _scale;

		// Floor to get integer voxel coordinates relative to the preprocessed grid's (0,0,0)
		int vx = static_cast<int>(Math::floor(local_pos.x - _model_min_corner.x));
		int vy = static_cast<int>(Math::floor(local_pos.y - _model_min_corner.y));
		int vz = static_cast<int>(Math::floor(local_pos.z - _model_min_corner.z));

		// Explicitly cast _model_dimensions components to int for clamping and index calculation
		int dim_x_int = static_cast<int>(_model_dimensions.x);
		int dim_y_int = static_cast<int>(_model_dimensions.y);
		int dim_z_int = static_cast<int>(_model_dimensions.z);

		// Clamp coordinates to be within the bounds of the preprocessed array
		vx = std::clamp(vx, 0, dim_x_int - 1);
		vy = std::clamp(vy, 0, dim_y_int - 1);
		vz = std::clamp(vz, 0, dim_z_int - 1);

		// Calculate 1D index
		size_t index = static_cast<size_t>(vx + vy * dim_x_int + vz * dim_x_int * dim_y_int);

		if (index < _preprocessed_voxels.size()) {
			return _preprocessed_voxels[index];
		}
		return _material; // Fallback material if index is problematic (shouldn't happen with clamping)
	}

	virtual std::pair<Vector3, Vector3> getApproximateWorldBounds() const override {
		_calculate_and_cache_local_bounds(); // Ensure bounds are cached

		// Add the SDFVoxEdit's own offset to the locally cached bounds to get world bounds
		return { _cached_scene_aabb_min_local + _offset, _cached_scene_aabb_max_local + _offset };
	}

	virtual Ref<ISDFEdit> clone() const override {
		Ref<SDFVoxEdit> new_edit;
		new_edit.instantiate();
		new_edit->set_file_path(_file_path);
		new_edit->set_offset(_offset);
		new_edit->set_material(_material);
		new_edit->set_scale(_scale); // Clone scale as well
		return new_edit;
	}


//protected:
//	static void _bind_methods();
};



// ----------------------------------------------------------------------------
// Per-chunk metadata: stores chunk-coords, SDF edits
// ----------------------------------------------------------------------------
struct ChunkMetadata {
	Vector3 chunkCoords;
	Vector<Ref<ISDFEdit>> sdfEdits;
	Vector<Ref<ISDFEdit>> UnprocessedEdits;
	ObjectID meshInstance; // ObjectID of the generated MeshInstance3D for this chunk
	bool is_dirty = false;
	bool generate_terrain;
	bool fresh = true;
	std::vector<uint8_t> voxels;
	ObjectID collisionShape;

};

#endif // TERRAIN_CHUNK_TYPES_H
