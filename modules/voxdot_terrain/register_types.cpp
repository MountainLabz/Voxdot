#include "register_types.h"
#include "terrain_chunk_types.h"
#include "core/object/class_db.h"
#include "voxdot_terrain.h"
#include "world_gen.h"
#include "voxel_material_properties.h"

void initialize_voxdot_terrain_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<VoxWorldStructure>();
	ClassDB::register_class<SharedVoxData>();
	ClassDB::register_class<VoxelMaterialProperties>();
	ClassDB::register_class<TerrainLayer>();
	ClassDB::register_class<Biome>();
	ClassDB::register_class<VoxdotTerrain>();
	
}

void uninitialize_voxdot_terrain_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Nothing to do here in this example.
}
