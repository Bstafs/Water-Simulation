#pragma once

#include "Includes.h"

class Particle 
{
public:
	Particle(XMFLOAT3 particlePos, XMFLOAT3 particleVelocity, float particleDensity, XMFLOAT3 particleAcceleration, float particleSmoothingRadius, XMFLOAT3 pressure);
	~Particle();
public:
	XMFLOAT3 position = {0,0,0};
	XMFLOAT3 velocity = { 0,0,0 };
	float density = 0.0f;
	float nearDensity = 0.0f;
	XMFLOAT3 acceleration = { 0,0,0 };
	XMFLOAT3 pressureForce = { 0,0,0 };
	float pressure = 0.0f;
	float nearPressure = 0.0f;
	float smoothingRadius = 2.5f;
	XMFLOAT3 predictiedPosition = XMFLOAT3();
};
