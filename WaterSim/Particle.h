#pragma once

#include "Includes.h"

class Particle 
{
public:
	Particle(float particleMass, float particleSize, XMFLOAT3 particlePos, XMFLOAT3 particleVelocity);
	~Particle();
private:
	static int particleCount;
public:
	// Particle Properties
	float mass = 0.0f;
	float size = 0.0f;
	float elasticity = 0.0f;
	float density = 0.0f;
	float pressure = 0.0f;
	// Vector 3 Forces
	XMFLOAT3 position = {0,0,0};
	XMFLOAT3 velocity = { 0,0,0 };
	XMFLOAT3 acceleration = { 0,0,0 };
	XMFLOAT3 force = { 0,0,0 };
	// Collision
	BoundingSphere sphere;
};
