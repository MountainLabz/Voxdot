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

	protected:
		static void _bind_methods() {
			ClassDB::bind_method(D_METHOD("set_terrain_layers", "layers"), &Biome::set_terrain_layers);
			ClassDB::bind_method(D_METHOD("get_terrain_layers"), &Biome::get_terrain_layers);
			ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "terrain_layers",
								 PROPERTY_HINT_ARRAY_TYPE, "TerrainLayer"),
					"set_terrain_layers", "get_terrain_layers");
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

		void set_noise(Ref<FastNoiseLite> p_noise) { noise = p_noise; }
		Ref<FastNoiseLite> get_noise() const { return noise; }
	};
