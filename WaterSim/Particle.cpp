#include "Particle.h"

int Particle::particleCount = 0;

Particle::Particle(float particleMass, float particleSize, XMFLOAT3 particlePos, XMFLOAT3 particleVelocity)
{
	mass = particleMass;
	size = particleSize;
	position = particlePos;
	velocity = particleVelocity;

	force = XMFLOAT3(0,0,0);
	acceleration = XMFLOAT3(0, 0, 0);
	density = 0.0f;
	pressure = 0.0f;
	particleID = particleCount++;
}

Particle::~Particle()
{

}


