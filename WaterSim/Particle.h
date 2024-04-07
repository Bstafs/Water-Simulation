#pragma once

#include "Includes.h"

class Particle 
{
public:
	Particle(XMFLOAT3 particlePos, XMFLOAT3 particleVelocity);
	~Particle();
private:
	static int particleCount;
public:
	// Vector 3 Forces
	XMFLOAT3 position = {0,0,0};
	XMFLOAT3 velocity = { 0,0,0 };
	// Collision
	BoundingSphere sphere;
};
