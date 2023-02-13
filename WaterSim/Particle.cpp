#include "Particle.h"

int Particle::particleCount = 0;

Particle::Particle(float particleMass, float particleSize, Vector3 particlePos, Vector3 particleVelocity)
{
	mass = particleMass;
	size = particleSize;
	position = particlePos;
	velocity = particleVelocity;

	force = Vector3(0,0,0);
	acceleration = Vector3(0, 0, 0);
	density = 0.0f;
	pressure = 0.0f;
	particleID = particleCount++;

	nextParticle = nullptr;
}

Particle::~Particle()
{

}
