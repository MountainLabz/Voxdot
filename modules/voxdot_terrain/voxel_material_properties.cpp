#include "voxel_material_properties.h"
#include "core/math/math_funcs.h" // For CLAMP

VoxelMaterialProperties::VoxelMaterialProperties() :
		metallicness(0.0f),
		emissiveness(0.0f) {
	// Default values
}

void VoxelMaterialProperties::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_metallicness", "metallicness"), &VoxelMaterialProperties::set_metallicness);
	ClassDB::bind_method(D_METHOD("get_metallicness"), &VoxelMaterialProperties::get_metallicness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "metallicness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_metallicness", "get_metallicness");

	ClassDB::bind_method(D_METHOD("set_emissiveness", "emissiveness"), &VoxelMaterialProperties::set_emissiveness);
	ClassDB::bind_method(D_METHOD("get_emissiveness"), &VoxelMaterialProperties::get_emissiveness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emissiveness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_emissiveness", "get_emissiveness");
}

void VoxelMaterialProperties::set_metallicness(float p_metallicness) {
	metallicness = CLAMP(p_metallicness, 0.0f, 1.0f);
	emit_changed(); // Notify editor/listeners of property change
}

float VoxelMaterialProperties::get_metallicness() const {
	return metallicness;
}

void VoxelMaterialProperties::set_emissiveness(float p_emissiveness) {
	emissiveness = CLAMP(p_emissiveness, 0.0f, 1.0f);
	emit_changed(); // Notify editor/listeners of property change
}

float VoxelMaterialProperties::get_emissiveness() const {
	return emissiveness;
}
