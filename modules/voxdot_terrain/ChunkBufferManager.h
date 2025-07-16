#pragma once
#ifndef CHUNK_BUFFER_MANAGER_H
#define CHUNK_BUFFER_MANAGER_H

// Core Godot includes
#include "core/object/class_db.h" // For ERR_FAIL_COND_MSG
#include "core/os/mutex.h" // Godot's Mutex (replaces std::mutex)
#include "core/templates/hash_map.h" // Godot's HashMap (replaces std::unordered_map)
#include "core/templates/vector.h" // Godot's Vector (replaces std::vector)

// Godot's RenderingDevice
#include "servers/rendering/rendering_device.h" // Crucial for low-level rendering

// Standard library includes for memcpy
#include <cstddef>
#include <cstdint>
#include <cstring> // For memcpy

// Manages a persistently-mapped SSBO-like ring buffer for quad data (uint64_t per quad)
// using Godot's RenderingDevice.
class ChunkBufferManager {
public:
	struct ChunkMeshInfo {
		uint32_t slot_offset; // Starting slot in buffer (one slot = one uint64_t quad)
		uint32_t quad_count; // Number of quads for this chunk
	};

	// Initialize the buffer to hold max_quads uint64_t entries.
	// Takes a RenderingDevice pointer, as this class directly uses its API.
	void initialize(RenderingDevice *p_rd, size_t max_quads) {
		// Validate the RenderingDevice pointer
		ERR_FAIL_COND_MSG(!p_rd, "RenderingDevice is null when initializing ChunkBufferManager.");
		rd = p_rd; // Assign the provided RenderingDevice instance

		total_slots = max_quads;
		buffer_size_bytes = max_quads * sizeof(uint64_t);

		// ← pick only the creation flags you need
		uint32_t creation_flags =
				RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT |
				RenderingDevice::BUFFER_CREATION_DEVICE_ADDRESS_BIT; // if you need device_address()

		// no 'usage' enum here—just pass an empty PackedByteArray or omit it
		RID buffer_rid = rd->storage_buffer_create(
				buffer_size_bytes,
				PackedByteArray(),
				RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT | RenderingDevice::BUFFER_CREATION_DEVICE_ADDRESS_BIT);
		ERR_FAIL_COND_MSG(!buffer_rid.is_valid(), "Failed to create GPU buffer");

		// Initialize our CPU-side buffer (`mapped_data`) that mimics the persistently mapped memory.
		// Data will be copied here first, then uploaded to the GPU buffer via `buffer_update`.
		mapped_data.resize(buffer_size_bytes);
		// Ensure all bytes are zeroed initially in the CPU-side buffer.
		memset(mapped_data.ptrw(), 0, buffer_size_bytes);

		write_cursor_bytes = 0; // Initialize write cursor for the ring buffer
	}

	// Stage quad data for a chunk: copy quads into the CPU-side ring buffer,
	// then upload the relevant portion to the GPU buffer via `RenderingDevice::buffer_update`.
	void stage_chunk_data(uint64_t chunk_key, const Vector<uint64_t> &p_quads) {
		// Ensure RenderingDevice is valid before attempting operations.
		ERR_FAIL_COND_MSG(!rd, "RenderingDevice is not available when staging chunk data.");

		// Use Godot's Mutex for thread safety during buffer operations.
		MutexLock lock(mutex);

		size_t quad_byte_count = p_quads.size() * sizeof(uint64_t);
		ERR_FAIL_COND_MSG(quad_byte_count == 0, "Attempted to stage empty quad data.");
		// Ensure the data size for a single chunk does not exceed the total buffer capacity.
		ERR_FAIL_COND_MSG(quad_byte_count > buffer_size_bytes, "Quad data size for chunk exceeds total buffer capacity.");

		// Implement ring buffer wrapping logic. If the current write plus new data
		// goes beyond the buffer size, wrap the cursor back to the beginning.
		if ((write_cursor_bytes + quad_byte_count) > buffer_size_bytes) {
			write_cursor_bytes = 0;
		}

		// Get a writable pointer to the internal data of Godot's `Vector<uint8_t>`.
		uint8_t *dest_ptr = mapped_data.ptrw() + write_cursor_bytes;

		// Copy quad data from the input `Vector<uint64_t>` to our CPU-side `Vector<uint8_t>`.
		// `p_quads.ptr()` gives a read-only pointer to the internal data of `p_quads`.
		memcpy(dest_ptr, p_quads.ptr(), quad_byte_count);

		// Update the GPU buffer. `buffer_update` is a general method for updating any buffer type.
		// It takes the buffer RID, offset, size, pointer to data, and an optional `p_no_dom_sync` flag (true for asynchronous update).
		rd->buffer_update(buffer_rid, write_cursor_bytes, quad_byte_count, dest_ptr);

		// Record mesh information (offset and quad count) in our hash map.
		// The offset is in terms of uint64_t slots, not bytes.
		mesh_infos[chunk_key] = {
			static_cast<uint32_t>(write_cursor_bytes / sizeof(uint64_t)),
			static_cast<uint32_t>(p_quads.size())
		};

		// Advance the write cursor for the next staging operation.
		write_cursor_bytes += quad_byte_count;
	}

	// Get the RID of the storage buffer. This RID can be used to bind the buffer
	// in shader uniform sets for rendering.
	RID get_buffer_rid() const {
		return buffer_rid;
	}

	// Retrieve the map of chunk keys to their mesh information.
	const HashMap<uint64_t, ChunkMeshInfo> &get_mesh_infos() const {
		return mesh_infos;
	}

	// Clear all staged mesh information and reset the write cursor.
	void clear() {
		mesh_infos.clear(); // Clear the hash map
		write_cursor_bytes = 0; // Reset ring buffer cursor
		// Optionally, zero out the CPU-side buffer for cleanliness/debugging.
		memset(mapped_data.ptrw(), 0, buffer_size_bytes);
	}

	// Destroy the GPU buffer and clear all associated resources.
	void destroy() {
		// Ensure RenderingDevice is valid before attempting to free resources.
		if (rd && buffer_rid.is_valid()) {
			rd->free(buffer_rid); // Release the GPU resource managed by RenderingDevice
			buffer_rid = RID(); // Invalidate the RID to prevent double-freeing
		}
		mapped_data.clear(); // Clear Godot's Vector holding CPU-side data
		mesh_infos.clear(); // Clear Godot's HashMap
		rd = nullptr; // Nullify the RenderingDevice pointer
	}

private:
	RenderingDevice *rd = nullptr; // Pointer to the Godot RenderingDevice instance
	RID buffer_rid; // Godot's resource ID for the GPU storage buffer
	Vector<uint8_t> mapped_data; // CPU-side buffer mimicking persistently mapped GPU memory
	size_t buffer_size_bytes = 0; // Total size of the GPU buffer in bytes
	size_t total_slots = 0; // Total number of uint64_t slots in the buffer
	size_t write_cursor_bytes = 0; // Current write position in the ring buffer (in bytes)
	Mutex mutex; // Godot's Mutex for ensuring thread-safe access
	HashMap<uint64_t, ChunkMeshInfo> mesh_infos; // Map from chunk ID to its mesh data info
};

#endif // CHUNK_BUFFER_MANAGER_H
