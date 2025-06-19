#pragma once

#include "Includes.h"

#include "MarchingCubeTable.h"
#include "Particle.h"


using namespace DirectX;
using namespace std;


class MarchingCubes
{
public:
	MarchingCubes();
	~MarchingCubes();


    void GenerateMarchingCubesMesh(
        const std::vector<float>& scalarField,
        int gridSizeX, int gridSizeY, int gridSizeZ,
        float cellSize,
        float isoLevel,
        std::vector<SimpleVertex>& outVertices,
        std::vector<DWORD>& outIndices);

	std::vector<float> GenerateScalarField(
		const std::vector<Particle*>& particles,
		int gridSizeX, int gridSizeY, int gridSizeZ,
		float cellSize,
		float smoothingRadius, float isoLevel, XMFLOAT3 gridOrigin);

private:

	XMFLOAT3 VertexInterp(float isoLevel, const XMFLOAT3& p1, const XMFLOAT3& p2, float valp1, float valp2);


};

