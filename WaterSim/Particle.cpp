#include "Particle.h"

int Particle::particleCount = 0;

Particle::Particle(XMFLOAT3 particlePos, XMFLOAT3 particleVelocity, float particleDensity, XMFLOAT3 particleAcceleration)
{
	position = particlePos;
	velocity = particleVelocity;
	density = particleDensity;
	acceleration = particleAcceleration;
}

Particle::~Particle()
{

}


