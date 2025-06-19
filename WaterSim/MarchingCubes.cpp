#include "MarchingCubes.h"

MarchingCubes::MarchingCubes()
{
	
}

MarchingCubes::~MarchingCubes()
{
	
}

XMFLOAT3 MarchingCubes::VertexInterp(float isoLevel, const XMFLOAT3& p1, const XMFLOAT3& p2, float valp1, float valp2)
{
	if (std::abs(isoLevel - valp1) < 0.00001f)
		return p1;
	if (std::abs(isoLevel - valp2) < 0.00001f)
		return p2;
	if (abs(valp1 - valp2) < 0.00001f) 
		return p1;

	float mu = (isoLevel - valp1) / (valp2 - valp1);
	return XMFLOAT3(
		p1.x + mu * (p2.x - p1.x),
		p1.y + mu * (p2.y - p1.y),
		p1.z + mu * (p2.z - p1.z)
	);
}

void MarchingCubes::GenerateMarchingCubesMesh(
	const std::vector<float>& scalarField,
	int gridSizeX, int gridSizeY, int gridSizeZ,
	float cellSize,
	float isoLevel,
	std::vector<SimpleVertex>& outVertices,
	std::vector<DWORD>& outIndices)
{

	static const int edgeIndexPairs[12][2] = {
	   {0,1}, {1,2}, {2,3}, {3,0},
	   {4,5}, {5,6}, {6,7}, {7,4},
	   {0,4}, {1,5}, {2,6}, {3,7}
	};

	static const XMFLOAT3 cornerOffsets[8] = {
	   {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
	   {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
	};

	for (int z = 0; z < gridSizeZ - 1; ++z)
	{
		for (int y = 0; y < gridSizeY - 1; ++y)
		{
			for (int x = 0; x < gridSizeX - 1; ++x)
			{
				int cubeIndex = 0;
				XMFLOAT3 cubeVerts[8];
				float cubeValues[8];

				for (int i = 0; i < 8; ++i)
				{
					int cornerX = x + static_cast<int>(cornerOffsets[i].x);
					int cornerY = y + static_cast<int>(cornerOffsets[i].y);
					int cornerZ = z + static_cast<int>(cornerOffsets[i].z);
					cubeVerts[i] = XMFLOAT3(
						cornerX * cellSize,
						cornerY * cellSize,
						cornerZ * cellSize
					);
					cubeValues[i] = scalarField[cornerX + cornerY * gridSizeX + cornerZ * gridSizeX * gridSizeY];
					if (cubeValues[i] < isoLevel)
						cubeIndex |= (1 << i);
				}

				int edgeFlags = EDGE_TABLE[cubeIndex];
				if (edgeFlags == 0) continue;

				XMFLOAT3 edgeVertices[12];
				for (int i = 0; i < 12; ++i)
				{
					if (edgeFlags & (1 << i))
					{
						int v1 = edgeIndexPairs[i][0];
						int v2 = edgeIndexPairs[i][1];
						edgeVertices[i] = VertexInterp(isoLevel, cubeVerts[v1], cubeVerts[v2], cubeValues[v1], cubeValues[v2]);
					}
				}

				for (int i = 0; TRI_TABLE[cubeIndex][i] != -1; i += 3)
				{
					uint32_t index0 = static_cast<uint32_t>(outVertices.size());
					outVertices.push_back(SimpleVertex{
						edgeVertices[TRI_TABLE[cubeIndex][i]],
						XMFLOAT3(0.0f, 0.0f, 0.0f), // Placeholder for normals
						XMFLOAT2(0.0f, 0.0f) // Placeholder for texture coordinates
						});

					outVertices.push_back(SimpleVertex{
						edgeVertices[TRI_TABLE[cubeIndex][i + 1]],
						XMFLOAT3(0.0f, 0.0f, 0.0f), // Placeholder for normals
						XMFLOAT2(0.0f, 0.0f) // Placeholder for texture coordinates
						});

					outVertices.push_back(SimpleVertex{
						edgeVertices[TRI_TABLE[cubeIndex][i + 2]],
						XMFLOAT3(0.0f, 0.0f, 0.0f), // Placeholder for normals
						XMFLOAT2(0.0f, 0.0f) // Placeholder for texture coordinates
						});

					outIndices.push_back(index0);
					outIndices.push_back(index0 + 1);
					outIndices.push_back(index0 + 2);
				}
			}
		}
	}
}


std::vector<float> MarchingCubes::GenerateScalarField(const std::vector<Particle*>& particles,
	int gridSizeX, int gridSizeY, int gridSizeZ,
	float cellSize,
	float smoothingRadius, float isoLevel,
	XMFLOAT3 gridOrigin)
{
	std::vector<float> scalarField(gridSizeX * gridSizeY * gridSizeZ, 0.0f);

	for (int z = 0; z < gridSizeZ; ++z)
	{
		for (int y = 0; y < gridSizeY; ++y)
		{
			for (int x = 0; x < gridSizeX; ++x)
			{

				XMFLOAT3 voxelPos = {
					gridOrigin.x + x * cellSize,
					gridOrigin.y + y * cellSize,
					gridOrigin.z + z * cellSize
				};

				float density = 0.0f;
				for (const Particle* particle : particles)
				{
					XMFLOAT3 r = {
						voxelPos.x - particle->position.x,
						voxelPos.y - particle->position.y,
						voxelPos.z - particle->position.z
					};

					float r2 = r.x * r.x + r.y * r.y + r.z * r.z;
					if (r2 < smoothingRadius * smoothingRadius)
					{
						float term = smoothingRadius * smoothingRadius - r2;
						float weight = term * term * term; // Poly6 without normalization constant
						density += particle->density * weight;
					}


				}

				scalarField[x + y * gridSizeX + z * gridSizeX * gridSizeY] = density;
			}
		}
	}
	return scalarField;
}