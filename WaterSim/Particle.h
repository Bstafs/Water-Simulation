#pragma once

#include "Includes.h"

class Particle 
{
public:
	Particle(XMFLOAT3 particlePos, XMFLOAT3 particleVelocity, float particleDensity, XMFLOAT3 particleAcceleration, float particleSmoothingRadius, XMFLOAT3 pressure);
	~Particle();
private:
	static int particleCount;
public:
	// Vector 3 Forces
	XMFLOAT3 position = {0,0,0};
	XMFLOAT3 velocity = { 0,0,0 };
	float density;
	XMFLOAT3 acceleration = { 0,0,0 };
	XMFLOAT3 pressureForce = { 0,0,0 };
	float smoothingRadius;
	// Collision
	BoundingSphere sphere;
};
