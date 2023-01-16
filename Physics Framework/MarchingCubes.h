#pragma once

#include <directxmath.h>
#include <d3d11_1.h>

#include <iostream>
#include <vector>
#include <random>
#include <functional>

#include "MarchingCubesTable.h"

using namespace DirectX;
using namespace std;

class MarchingCubes
{
private:
	MarchingCubes();

	void MarchingCubesGrid();
	void MarchingCubesMesh();
	void MarchingCubesGenerate();

private:

};

