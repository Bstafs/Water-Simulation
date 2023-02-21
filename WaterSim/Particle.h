#pragma once

#include "Vector3.h"
#include "Constants.h"

class Particle 
{
public:
	Particle(float particleMass, float particleSize, Vector3 particlePos, Vector3 particleVelocity);
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
	int particleID = 0;
	// Vector 3 Forces
	Vector3 position = {0,0,0};
	Vector3 velocity = { 0,0,0 };
	Vector3 acceleration = { 0,0,0 };
	Vector3 force = { 0,0,0 };

	// Particle Linked List
	Particle* nextParticle;

	// Functions For SPH particles such as distance
	float Distance(Particle* other);

	// As mentioned in Realtime Particle-Based Fluid Simulation, Each particle needs to use different Smoothing Kernel.
	float SmoothingKernel(Particle* other);

	// Calculate Gradient of Each Particle
	Vector3 GradientKernel(Particle* other);

	// Getters and Setters for Particle Variables
	float GetDensity() { return density; }
	void SetDensity(float dens) { density = dens; }

	float GetMass() { return mass; }
	void SetMass(float mas) { mass = mas; }

	float GetPressure() { return pressure; }
	void SetPressure(float press) { pressure = press; }
};
