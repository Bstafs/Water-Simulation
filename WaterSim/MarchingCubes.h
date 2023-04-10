#pragma once

#include <directxmath.h>
#include <d3d11_1.h>
#include <functional>

#include "MarchingCubeTable.h"

using namespace DirectX;
using namespace std;

class MarchingCubes
{
public:
	MarchingCubes();

	void MarchingCubesGrid();
	void MarchingCubesMesh();
	void MarchingCubesGenerate();

private:

};

