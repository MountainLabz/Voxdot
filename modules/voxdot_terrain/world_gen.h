#pragma once

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


// NEW: VoxWorldStructure Resource Definition
class VoxWorldStructure : public Resource {
	GDCLASS(VoxWorldStructure, Resource);

public:
	enum SpawnType {
		SPAWN_ONCE,
		SPAWN_RANDOM,
	};

private:
	String model_key;
	float min_altitude = -10000.0f;
	float max_altitude = 10000.0f;
	Ref<FastNoiseLite> noise_layer;
	float required_noise_value_min = -1.0f;
	float required_noise_value_max = 1.0f;
	float spawn_frequency = 0.05f;
	int random_seed = 0;
	SpawnType spawn_type = SPAWN_RANDOM;
	Vector3 spawn_position; // For SPAWN_ONCE
	bool ground_aligned = false; // Ground alignment flag
	Vector3 position_offset; // Position offset

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_model_key", "key"), &VoxWorldStructure::set_model_key);
		ClassDB::bind_method(D_METHOD("get_model_key"), &VoxWorldStructure::get_model_key);
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "model_key"), "set_model_key", "get_model_key");

		ClassDB::bind_method(D_METHOD("set_min_altitude", "altitude"), &VoxWorldStructure::set_min_altitude);
		ClassDB::bind_method(D_METHOD("get_min_altitude"), &VoxWorldStructure::get_min_altitude);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_altitude"), "set_min_altitude", "get_min_altitude");

		ClassDB::bind_method(D_METHOD("set_max_altitude", "altitude"), &VoxWorldStructure::set_max_altitude);
		ClassDB::bind_method(D_METHOD("get_max_altitude"), &VoxWorldStructure::get_max_altitude);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_altitude"), "set_max_altitude", "get_max_altitude");

		ClassDB::bind_method(D_METHOD("set_noise_layer", "noise"), &VoxWorldStructure::set_noise_layer);
		ClassDB::bind_method(D_METHOD("get_noise_layer"), &VoxWorldStructure::get_noise_layer);
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_layer", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise_layer", "get_noise_layer");

		ClassDB::bind_method(D_METHOD("set_required_noise_value_min", "value"), &VoxWorldStructure::set_required_noise_value_min);
		ClassDB::bind_method(D_METHOD("get_required_noise_value_min"), &VoxWorldStructure::get_required_noise_value_min);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "required_noise_value_min", PROPERTY_HINT_RANGE, "-1,1"), "set_required_noise_value_min", "get_required_noise_value_min");

		ClassDB::bind_method(D_METHOD("set_required_noise_value_max", "value"), &VoxWorldStructure::set_required_noise_value_max);
		ClassDB::bind_method(D_METHOD("get_required_noise_value_max"), &VoxWorldStructure::get_required_noise_value_max);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "required_noise_value_max", PROPERTY_HINT_RANGE, "-1,1"), "set_required_noise_value_max", "get_required_noise_value_max");

		ClassDB::bind_method(D_METHOD("set_spawn_frequency", "frequency"), &VoxWorldStructure::set_spawn_frequency);
		ClassDB::bind_method(D_METHOD("get_spawn_frequency"), &VoxWorldStructure::get_spawn_frequency);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spawn_frequency", PROPERTY_HINT_RANGE, "0,1"), "set_spawn_frequency", "get_spawn_frequency");

		ClassDB::bind_method(D_METHOD("set_random_seed", "seed"), &VoxWorldStructure::set_random_seed);
		ClassDB::bind_method(D_METHOD("get_random_seed"), &VoxWorldStructure::get_random_seed);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "random_seed"), "set_random_seed", "get_random_seed");

		ClassDB::bind_method(D_METHOD("set_spawn_type", "type"), &VoxWorldStructure::set_spawn_type);
		ClassDB::bind_method(D_METHOD("get_spawn_type"), &VoxWorldStructure::get_spawn_type);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "spawn_type", PROPERTY_HINT_ENUM, "Once,Random"), "set_spawn_type", "get_spawn_type");

		ClassDB::bind_method(D_METHOD("set_spawn_position", "position"), &VoxWorldStructure::set_spawn_position);
		ClassDB::bind_method(D_METHOD("get_spawn_position"), &VoxWorldStructure::get_spawn_position);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "spawn_position"), "set_spawn_position", "get_spawn_position");

		ClassDB::bind_method(D_METHOD("set_ground_aligned", "aligned"), &VoxWorldStructure::set_ground_aligned);
		ClassDB::bind_method(D_METHOD("get_ground_aligned"), &VoxWorldStructure::get_ground_aligned);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ground_aligned"), "set_ground_aligned", "get_ground_aligned");

		ClassDB::bind_method(D_METHOD("set_position_offset", "offset"), &VoxWorldStructure::set_position_offset);
		ClassDB::bind_method(D_METHOD("get_position_offset"), &VoxWorldStructure::get_position_offset);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position_offset"), "set_position_offset", "get_position_offset");

		BIND_ENUM_CONSTANT(SPAWN_ONCE);
		BIND_ENUM_CONSTANT(SPAWN_RANDOM);
	}

public:
	void set_model_key(const String &p_key) { model_key = p_key; }
	String get_model_key() const { return model_key; }

	void set_min_altitude(float p_alt) { min_altitude = p_alt; }
	float get_min_altitude() const { return min_altitude; }

	void set_max_altitude(float p_alt) { max_altitude = p_alt; }
	float get_max_altitude() const { return max_altitude; }

	void set_noise_layer(const Ref<FastNoiseLite> &p_noise) { noise_layer = p_noise; }
	Ref<FastNoiseLite> get_noise_layer() const { return noise_layer; }

	void set_required_noise_value_min(float p_val) { required_noise_value_min = p_val; }
	float get_required_noise_value_min() const { return required_noise_value_min; }

	void set_required_noise_value_max(float p_val) { required_noise_value_max = p_val; }
	float get_required_noise_value_max() const { return required_noise_value_max; }

	void set_spawn_frequency(float p_freq) { spawn_frequency = CLAMP(p_freq, 0.0f, 1.0f); }
	float get_spawn_frequency() const { return spawn_frequency; }

	void set_random_seed(int p_seed) { random_seed = p_seed; }
	int get_random_seed() const { return random_seed; }

	void set_spawn_type(SpawnType p_type) { spawn_type = p_type; }
	SpawnType get_spawn_type() const { return spawn_type; }

	void set_spawn_position(const Vector3 &p_pos) { spawn_position = p_pos; }
	Vector3 get_spawn_position() const { return spawn_position; }

	void set_ground_aligned(bool p_aligned) { ground_aligned = p_aligned; }
	bool get_ground_aligned() const { return ground_aligned; }

	void set_position_offset(const Vector3 &p_offset) { position_offset = p_offset; }
	Vector3 get_position_offset() const { return position_offset; }

	VoxWorldStructure() {}
	~VoxWorldStructure() {}
};

VARIANT_ENUM_CAST(VoxWorldStructure::SpawnType);

class TerrainLayer : public Resource {
	GDCLASS(TerrainLayer, Resource);
	Ref<FastNoiseLite> noise;
	float noise_max;
	float noise_base;
	int material_type;
	bool ThreeDimensional;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_noise", "noise"), &TerrainLayer::set_noise);
		ClassDB::bind_method(D_METHOD("get_noise"), &TerrainLayer::get_noise);
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise", "get_noise");

		ClassDB::bind_method(D_METHOD("set_noise_max", "v"), &TerrainLayer::set_noise_max);
		ClassDB::bind_method(D_METHOD("get_noise_max"), &TerrainLayer::get_noise_max);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_max"), "set_noise_max", "get_noise_max");

		ClassDB::bind_method(D_METHOD("set_noise_base", "v"), &TerrainLayer::set_noise_base);
		ClassDB::bind_method(D_METHOD("get_noise_base"), &TerrainLayer::get_noise_base);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_base"), "set_noise_base", "get_noise_base");

		ClassDB::bind_method(D_METHOD("set_material_type", "v"), &TerrainLayer::set_material_type);
		ClassDB::bind_method(D_METHOD("get_material_type"), &TerrainLayer::get_material_type);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "material_type"), "set_material_type", "get_material_type");

		ClassDB::bind_method(D_METHOD("set_dimension", "v"), &TerrainLayer::set_dimension);
		ClassDB::bind_method(D_METHOD("get_dimension"), &TerrainLayer::get_dimension);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Is_ThreeDimentional"), "set_dimension", "get_dimension");
	}

public:
	TerrainLayer() :
			ThreeDimensional(false),
			noise_max(30.0f),
			noise_base(0.0f),
			material_type(1) {
		// Instantiate noise with a default FastNoiseLite resource
		noise.instantiate();
		noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
		noise->set_frequency(0.05f);
		noise->set_fractal_octaves(3);
		noise->set_fractal_lacunarity(2.0f);
		noise->set_fractal_gain(0.5f);
		noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	}

	void set_noise(Ref<FastNoiseLite> p_noise) { noise = p_noise; }
	Ref<FastNoiseLite> get_noise() const { return noise; }

	void set_noise_max(float p_noise_max) { noise_max = p_noise_max; }
	float get_noise_max() const { return noise_max; }

	void set_noise_base(float p_noise_base) { noise_base = p_noise_base; }
	float get_noise_base() const { return noise_base; }

	void set_material_type(int p_material_type) { material_type = p_material_type; }
	int get_material_type() const { return material_type; }

	void set_dimension(bool p_dimension) { ThreeDimensional = p_dimension; }
	int get_dimension() const { return ThreeDimensional; }
};

class Biome : public Resource {
		GDCLASS(Biome, Resource);
		Ref<FastNoiseLite> noise;
		Vector<Ref<TerrainLayer>> terrain_layers;
		Vector<Ref<VoxWorldStructure>> structures;

	protected:
		static void _bind_methods() {
			ClassDB::bind_method(D_METHOD("set_terrain_layers", "layers"), &Biome::set_terrain_layers);
			ClassDB::bind_method(D_METHOD("get_terrain_layers"), &Biome::get_terrain_layers);
			ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "terrain_layers",
								 PROPERTY_HINT_ARRAY_TYPE, "TerrainLayer"),
					"set_terrain_layers", "get_terrain_layers");

			ClassDB::bind_method(D_METHOD("set_structures", "structures"), &Biome::set_structures);
			ClassDB::bind_method(D_METHOD("get_structures"), &Biome::get_structures);
			ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structures", PROPERTY_HINT_ARRAY_TYPE, "VoxWorldStructure"), "set_structures", "get_structures");
		}

	public:
		void set_terrain_layers(const Array &layers) {
			terrain_layers.clear();
			for (int i = 0; i < layers.size(); ++i) {
				Variant v = layers[i];
				Ref<TerrainLayer> tl = v;
				if (tl.is_valid()) {
					// user assigned a resource
					terrain_layers.push_back(tl);
				} else {
					// auto‑instantiate a fresh TerrainLayer
					Ref<TerrainLayer> new_tl = memnew(TerrainLayer);
					terrain_layers.push_back(new_tl);
				}
			}
		}

		Array get_terrain_layers() const {
			Array out;
			for (auto &tl : terrain_layers) {
				out.append(tl);
			}
			return out;
		}

		void set_structures(const Array &structure) {
			structures.clear();
			for (int i = 0; i < structure.size(); ++i) {
				Variant v = structure[i];
				Ref<VoxWorldStructure> tl = v;
				if (tl.is_valid()) {
					// user assigned a resource
					structures.push_back(tl);
				} else {
					// auto‑instantiate a fresh TerrainLayer
					Ref<VoxWorldStructure> new_tl = memnew(VoxWorldStructure);
					structures.push_back(new_tl);
				}
			}
		}

		Array get_structures() const {
			Array out;
			for (auto &tl : structures) {
				out.append(tl);
			}
			return out;
		}

		void set_noise(Ref<FastNoiseLite> p_noise) { noise = p_noise; }
		Ref<FastNoiseLite> get_noise() const { return noise; }
	};
