// ChunkHandler_godot.h
#pragma once

#include "ChunkBufferManager.h"
#include "FastNoiseLite.h"
#include "UniversalPool.h"
#include "core/math/vector3i.h"
#include "core/math/vector4i.h"
//#include "core/variant/packed_byte_array.h"

#include "mesher.h"
#include "servers/rendering/rendering_device.h"
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

//using godot::PackedByteArray;
//using godot::RenderingDevice;
//using godot::RID;



struct ISDFEdit {
	virtual float get_signed_distance(const Vector3 &point) const = 0;
	virtual uint8_t get_material() const = 0;
	virtual std::pair<Vector3, Vector3> get_approximate_world_bounds() const = 0;
	virtual std::unique_ptr<ISDFEdit> clone() const = 0;
	virtual ~ISDFEdit() = default;
};

struct SDFSphereEdit : public ISDFEdit {
	Vector3 center;
	float radius;
	uint8_t material;
	SDFSphereEdit(const Vector3 &c, float r, uint8_t m) :
			center(c), radius(r), material(m) {}
	float get_signed_distance(const Vector3 &p) const override;
	uint8_t get_material() const override { return material; }
	std::pair<Vector3, Vector3> get_approximate_world_bounds() const override;
	std::unique_ptr<ISDFEdit> clone() const override;
};
// ... SDFCubeEdit similar


// ----------------------------------------------------------------------------
// Hash & equality for glm::ivec3 so we can use it as key in std::unordered_map
// ----------------------------------------------------------------------------
struct IVec3Hash {
	size_t operator()(Vector3i const &v) const noexcept {
		uint64_t x = static_cast<uint64_t>(v.x);
		uint64_t y = static_cast<uint64_t>(v.y);
		uint64_t z = static_cast<uint64_t>(v.z);
		uint64_t h = (x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u);
		return static_cast<size_t>(h);
	}
};
struct IVec3Eq {
	bool operator()(Vector3i const &a, Vector3i const &b) const noexcept {
		return a.x == b.x && a.y == b.y && a.z == b.z;
	}
};


struct ChunkData {
	Vector4i offset; // chunk coords + padding
	int first;
	int count;
	int padding1;
	int padding2;
};

// ----------------------------------------------------------------------------
// Per-chunk metadata: stores SSBO-offset + quad-count + chunk-coords
// ----------------------------------------------------------------------------
struct ChunkMetadata {
	Vector3i chunkCoords;
	int poolNodeID;
	uint32_t quadCount;
	uint32_t ssboSlotOffset;
	std::vector<std::unique_ptr<ISDFEdit>> sdfEdits; // Now stores unique_ptrs to base interface
};


class ChunkHandler {
public:
	ChunkHandler();
	~ChunkHandler();

	bool init(RenderingDevice *device, uint32_t max_total_quads);
	void destroy();

	bool add_or_update_chunk(const Vector3i &coords, const std::vector<uint64_t> &quads);
	void remove_chunk(const Vector3i &coords);
	void clear_all();

	void bind_quads_ssbo(uint32_t binding_point) const;
	void bind_metadata_ssbo(uint32_t binding_point);

	size_t get_loaded_chunk_count() const;
	size_t retrieve_firsts_and_counts(std::vector<uint32_t> &firsts,
			std::vector<uint32_t> &counts) const;

private:
	void prepare_chunk_mesh(const Vector3i &coords);
	void prepare_metadata_buffer();

	UniversalPool<uint64_t, true> *pool = nullptr;
	ChunkBufferManager buffer_mgr;
	RID metadata_rid;
	RenderingDevice *rd = nullptr;

	std::unordered_map<Vector3i, ChunkMetadata, IVec3Hash, IVec3Eq> chunk_map;
	std::vector<ChunkData> temp_chunk_data;
};
