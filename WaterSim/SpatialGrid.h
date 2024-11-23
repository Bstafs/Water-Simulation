#pragma once
#include "Includes.h"

using namespace Microsoft::WRL;

namespace std {
	template<typename T>
	inline void hash_combine(std::size_t& seed, const T& v) {
		seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	template<typename... Types>
	struct hash<std::tuple<Types...>> {
		size_t operator()(const std::tuple<Types...>& key) const {
			return hash_tuple(key, std::index_sequence_for<Types...>{});
		}

	private:
		template<typename Tuple, size_t... Indices>
		size_t hash_tuple(const Tuple& tuple, std::index_sequence<Indices...>) const {
			size_t seed = 0;
			(hash_combine(seed, std::get<Indices>(tuple)), ...);
			return seed;
		}
	};
}

struct SpatialGrid {
	float cellSize;
	std::unordered_map<std::tuple<int, int, int>, std::vector<int>> grid;

	SpatialGrid(float cellSize) : cellSize(cellSize) {}

	void Clear() 
	{
		grid.clear();
	}

	std::tuple<int, int, int> GetCellIndex(const DirectX::XMFLOAT3& position) const 
	{
		int x = static_cast<int>(std::floor(position.x / cellSize));
		int y = static_cast<int>(std::floor(position.y / cellSize));
		int z = static_cast<int>(std::floor(position.z / cellSize));
		return std::make_tuple(x, y, z);
	}

	void AddParticle(int particleIndex, const DirectX::XMFLOAT3& position) 
	{
		auto cellIndex = GetCellIndex(position);
		grid[cellIndex].push_back(particleIndex);
	}

	// Get particles in neighboring cells
	std::vector<int> GetNeighboringParticles(const XMFLOAT3& position) const 
	{
		std::vector<int> neighbors;
		auto baseCell = GetCellIndex(position);

		// Check this cell and its neighbors (3x3x3 region)
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dz = -1; dz <= 1; ++dz) {
					auto neighborCell = std::make_tuple(
						std::get<0>(baseCell) + dx,
						std::get<1>(baseCell) + dy,
						std::get<2>(baseCell) + dz
					);

					if (grid.find(neighborCell) != grid.end()) {
						neighbors.insert(neighbors.end(), grid.at(neighborCell).begin(), grid.at(neighborCell).end());
					}
				}
			}
		}

		return neighbors;
	}
};

