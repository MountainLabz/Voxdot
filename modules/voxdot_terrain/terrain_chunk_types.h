#ifndef TERRAIN_CHUNK_TYPES_H
#define TERRAIN_CHUNK_TYPES_H


#include <iostream> // Required header for std::cout
#include <modules/noise/fastnoise_lite.h>
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/math/math_funcs.h" // Still useful for other Math functions like abs, floor, ceil
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include <algorithm> // For std::min and std::max
#include "ogt_vox.h" // Include ogt_vox.h here as SDFVoxEdit will use it
#include <string> // For std::string in SDFVoxEdit
#include <vector> // For std::vector in SDFVoxEdit (to store voxel data from .vox)
#include <fstream> // For file reading in SDFVoxEdit
#include <limits> // For numeric_limits in SDFVoxEdit

class SharedVoxData : public RefCounted {
	GDCLASS(SharedVoxData, RefCounted);

public:
	std::vector<uint8_t> voxels; // The dense voxel grid
	Vector3 dimensions; // Dimensions of the grid
	Vector3 min_corner; // The local-space minimum corner of the model

protected:
	static void _bind_methods() {} // Required for GDCLASS
};


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
	String _model_key; // MODIFIED: Replaced _file_path with _model_key
	Vector3 _offset;
	uint8_t _material;
	float _scale = 1.0f;

	Ref<SharedVoxData> _shared_data;

public:
	// Setters and Getters for properties
	void set_model_key(const String &p_key) { _model_key = p_key; }
	String get_model_key() const { return _model_key; }
	void set_offset(const Vector3 &p_offset) { _offset = p_offset; }
	Vector3 get_offset() const { return _offset; }
	void set_material(uint8_t p_material) { _material = p_material; }
	uint8_t get_material() const { return _material; }
	void set_scale(float p_scale) { _scale = p_scale; }
	float get_scale() const { return _scale; }

	// NEW: Method to link this edit to the shared data from the cache
	void set_shared_data(const Ref<SharedVoxData> &p_data) {
		_shared_data = p_data;
	}

	virtual float getSignedDistance(const Vector3 &p_pos) const override {
		if (_shared_data.is_null()) {
			return 1.0f; // No data, so it's air
		}

		// Transform world position to the model's local grid coordinates
		Vector3 local_pos = (p_pos - _offset) / _scale;
		int vx = static_cast<int>(Math::floor(local_pos.x - _shared_data->min_corner.x));
		int vy = static_cast<int>(Math::floor(local_pos.y - _shared_data->min_corner.y));
		int vz = static_cast<int>(Math::floor(local_pos.z - _shared_data->min_corner.z));

		// Check bounds
		int dim_x = static_cast<int>(_shared_data->dimensions.x);
		int dim_y = static_cast<int>(_shared_data->dimensions.y);
		int dim_z = static_cast<int>(_shared_data->dimensions.z);
		if (vx < 0 || vx >= dim_x || vy < 0 || vy >= dim_y || vz < 0 || vz >= dim_z) {
			return 1.0f; // Outside bounds
		}

		// Lookup in the shared voxel data
		size_t index = static_cast<size_t>(vx + vy * dim_x + vz * dim_x * dim_y);
		if (index < _shared_data->voxels.size()) {
			return _shared_data->voxels[index] != 0 ? -1.0f : 1.0f;
		}
		return 1.0f;
	}

	virtual uint8_t getMaterial(const Vector3 &p_pos) const override {
		if (_shared_data.is_null()) {
			return 0;
		}

		Vector3 local_pos = (p_pos - _offset) / _scale;
		int vx = static_cast<int>(Math::floor(local_pos.x - _shared_data->min_corner.x));
		int vy = static_cast<int>(Math::floor(local_pos.y - _shared_data->min_corner.y));
		int vz = static_cast<int>(Math::floor(local_pos.z - _shared_data->min_corner.z));

		int dim_x = static_cast<int>(_shared_data->dimensions.x);
		int dim_y = static_cast<int>(_shared_data->dimensions.y);
		int dim_z = static_cast<int>(_shared_data->dimensions.z);

		if (vx < 0 || vx >= dim_x || vy < 0 || vy >= dim_y || vz < 0 || vz >= dim_z) {
			return 0; // Air
		}

		size_t index = static_cast<size_t>(vx + vy * dim_x + vz * dim_x * dim_y);
		if (index < _shared_data->voxels.size()) {
			uint8_t mat_index = _shared_data->voxels[index];
			if (_material != 0) { // Single-material override mode
				return (mat_index != 0) ? _material : 0;
			}
			return mat_index; // Voxel's own material
		}
		return 0; // Air
	}

	virtual std::pair<Vector3, Vector3> getApproximateWorldBounds() const override {
		if (_shared_data.is_null()) {
			return { _offset, _offset };
		}
		// The bounds are calculated from the shared data's dimensions, scaled, and offset
		Vector3 min_local = _shared_data->min_corner * _scale;
		Vector3 max_local = (_shared_data->min_corner + _shared_data->dimensions) * _scale;
		return { min_local + _offset, max_local + _offset };
	}

	virtual Ref<ISDFEdit> clone() const override {
		Ref<SDFVoxEdit> new_edit;
		new_edit.instantiate();
		new_edit->set_model_key(_model_key); // MODIFIED
		new_edit->set_offset(_offset);
		new_edit->set_material(_material);
		new_edit->set_scale(_scale);
		new_edit->set_shared_data(_shared_data);
		return new_edit;
	}
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
