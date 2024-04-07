#include "Particle.h"

int Particle::particleCount = 0;

Particle::Particle(XMFLOAT3 particlePos, XMFLOAT3 particleVelocity)
{
	position = particlePos;
	velocity = particleVelocity;
}

Particle::~Particle()
{

}


