#include "register_types.h"

#include "core/object/class_db.h"
#include "voxdot_terrain.h"

void initialize_voxdot_terrain_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<VoxdotTerrain>();
}

void uninitialize_voxdot_terrain_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Nothing to do here in this example.
}
