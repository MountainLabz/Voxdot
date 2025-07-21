#ifndef UNIVERSAL_POOL_H
#define UNIVERSAL_POOL_H

#include <stdint.h>
#include <vector> // Replaced VectorF with std::vector
#include <numeric> // For std::iota (used in IDPool replacement)
#include <algorithm> // For std::remove_if (used in IDPool replacement)

// Simple IDPool replacement using std::vector
class IDPool {
public:
	IDPool() : next_id(0) {}

	int allocate() {
		if (!free_ids.empty()) {
			int id = free_ids.back();
			free_ids.pop_back();
			return id;
		}
		return next_id++;
	}

	void deallocate(int id) {
		free_ids.push_back(id);
	}

	void reset() {
		free_ids.clear();
		next_id = 0;
	}

private:
	std::vector<int> free_ids;
	int next_id;
};


struct MemoryBlock {
	uint32_t position;
	uint32_t size;
};

template <typename T, bool AllocateNodes = true>
class UniversalPool {
public:
	UniversalPool(uint32_t capacity, bool ownsMemory) : capacity(capacity) {
		if (ownsMemory) memory = new T[capacity];
		// No need to reserve usedNodes here directly,
		// as it's grown dynamically in allocate() for AllocateNodes == true.
		reset();
	}

	~UniversalPool() {
		delete[] memory;
	}

	void reset() {
		usedNodeAllocator.reset();
		usedNodes.clear(); // Clear usedNodes when resetting
		freeNodes.clear();

		freeNodes.push_back(MemoryNode{ 0, capacity, -1, -1 });
	}

	void setEndPadding(uint32_t size) {
		freeNodes[0].size -= size;
	}

	void deallocate(int nodeID) {
		MemoryNode& node = usedNodes[nodeID]; // This assumes nodeID is always valid for usedNodes' current size

		int leftNodeID = node.leftID & (~IS_FREE_NODE);
		MemoryNode* leftFreeNode = node.leftID >= IS_FREE_NODE ? &freeNodes[leftNodeID] : nullptr;

		int rightNodeID = node.rightID & (~IS_FREE_NODE);
		MemoryNode* rightFreeNode = node.rightID >= IS_FREE_NODE ? &freeNodes[rightNodeID] : nullptr;

		if (leftFreeNode != nullptr) {
			leftFreeNode->size += node.size;

			if (rightFreeNode == nullptr) {
				leftFreeNode->rightID = node.rightID;
				if (node.rightID != -1) usedNodes[node.rightID].leftID = node.leftID;
			}
			else {
				leftFreeNode->size += rightFreeNode->size;

				leftFreeNode->rightID = rightFreeNode->rightID;
				if (rightFreeNode->rightID != -1) usedNodes[rightFreeNode->rightID].leftID = node.leftID;

				removeFreeNode(rightNodeID);
			}
		}

		if (rightFreeNode != nullptr && leftFreeNode == nullptr) {
			rightFreeNode->position = node.position;
			rightFreeNode->size += node.size;

			rightFreeNode->leftID = node.leftID;
			if (node.leftID != -1) usedNodes[node.leftID].rightID = node.rightID;
		}

		if (leftFreeNode == nullptr && rightFreeNode == nullptr) {
			int freeNodeID = static_cast<int>(freeNodes.size()) | IS_FREE_NODE;
			freeNodes.push_back(node);

			if (node.leftID != -1) usedNodes[node.leftID].rightID = freeNodeID;
			if (node.rightID != -1) usedNodes[node.rightID].leftID = freeNodeID;
		}

		if constexpr (AllocateNodes) usedNodeAllocator.deallocate(nodeID);
	}

	bool allocate(int& nodeID, uint32_t size) {
		if (!seekFreeNode(size)) return false;
		MemoryNode& freeNode = freeNodes[searchID];

		if constexpr (AllocateNodes) {
			nodeID = usedNodeAllocator.allocate();
			// Crucial fix: Ensure usedNodes is large enough to hold nodeID
			if (static_cast<size_t>(nodeID) >= usedNodes.size()) {
				usedNodes.resize(nodeID + 1); // Resize to accommodate the new nodeID
			}
		}
		MemoryNode& node = usedNodes.at(nodeID); // .at() will now work correctly after resize
		node.position = freeNode.position;
		node.size = size;

		node.leftID = freeNode.leftID;
		if (node.leftID != -1) usedNodes[node.leftID].rightID = nodeID;

		freeNode.size -= size;
		if (freeNode.size == 0) {
			node.rightID = freeNode.rightID;
			if (node.rightID != -1) usedNodes[node.rightID].leftID = nodeID;

			removeFreeNode(searchID);
		}
		else {
			freeNode.position += size;

			freeNode.leftID = nodeID;
			node.rightID = searchID | IS_FREE_NODE;
		}

		return true;
	}

	MemoryBlock getBlock(int nodeID) {
		MemoryNode& node = usedNodes[nodeID];
		return MemoryBlock{ node.position, node.size };
	}

	T* getAddress(uint32_t position) {
		return memory + position;
	}
protected:
	T* memory = nullptr;
	uint32_t capacity;

	struct MemoryNode {
		uint32_t position, size;
		int leftID, rightID;
	};

	static constexpr int IS_FREE_NODE = 1 << 30;
	std::vector<MemoryNode> freeNodes, usedNodes;
	IDPool usedNodeAllocator;

	int searchID = 0;
	bool seekFreeNode(uint32_t size) {
		for (int i = 0; i < 2; i++) {
			for (; searchID < static_cast<int>(freeNodes.size()); searchID++) {
				if (freeNodes[searchID].size >= size) return true;
			}
			searchID = 0;
		}

		return false;
	}

	void removeFreeNode(int nodeID) {
		freeNodes[nodeID] = freeNodes.back();
		freeNodes.pop_back();
		if (nodeID == static_cast<int>(freeNodes.size())) return;

		MemoryNode& node = freeNodes[nodeID];
		if (node.leftID != -1) usedNodes[node.leftID].rightID = nodeID | IS_FREE_NODE;
		if (node.rightID != -1) usedNodes[node.rightID].leftID = nodeID | IS_FREE_NODE;
	}
};

#endif
