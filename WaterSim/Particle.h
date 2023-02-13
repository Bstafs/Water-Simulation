#pragma once

#include "Vector3.h"

class Particle 
{
public:
	Particle(float particleMass, float particleSize, Vector3 particlePos, Vector3 particleVelocity);
	~Particle();
private:
	static int particleCount;
public:
	// Particle Properties
	float mass;
	float size;
	float elasticity;

	// Vector 3 Forces
	Vector3 position;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 force;

	// Particle Linked List
	Particle* nextParticle;
	float density;
	float pressure;
	float particleID;

};
