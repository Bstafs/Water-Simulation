#include "Particle.h"

Particle::Particle(XMFLOAT3 particlePos, XMFLOAT3 particleVelocity, float particleDensity, XMFLOAT3 particleAcceleration, float particleSmoothingRadius, XMFLOAT3 pressure)
{
	position = particlePos;
	velocity = particleVelocity;
	density = particleDensity;
	acceleration = particleAcceleration;
	smoothingRadius = particleSmoothingRadius;
	pressureForce = pressure;
}

Particle::~Particle()
{

}


