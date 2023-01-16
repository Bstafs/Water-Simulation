#pragma once
#include<string>
#include <directxmath.h>
#include <d3d11_1.h>
#include "vector3d.h"
using namespace DirectX;
using namespace std;

class ParticleSystem
{
public:
	ParticleSystem(int particleLimit, vector3d SystemLocation, string name);

	int numParticles = 0;
	int 
};

