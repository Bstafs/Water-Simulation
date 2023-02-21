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

float Particle::Distance(Particle* other) 
{
	float xPos = position.x - other->position.x;
	float yPos = position.y - other->position.y;
	float zPos = position.z - other->position.z;

	return sqrt(xPos * xPos + yPos * yPos + zPos * zPos);
}

float Particle::SmoothingKernel(Particle* other) 
{
	float radius = Distance(other);
	if(radius >= 0 && radius <= size)
	{
		float temp = radius / size;
		return 15.0f / (PI * pow(size, 3) * (2.0f / 3.0f - temp * temp + 0.5f * pow(temp, 3)));
	}
	else
	{
		return 0.0f;
	}
}

Vector3 Particle::GradientKernel(Particle* other)
{
	float radius = Distance(other);
	Vector3 gradient = { 0,0,0 };

	if (radius >= 0 && radius <= size)
	{
		float temp = radius / size;
		float gradientCoefficient = -45.0f / (PI * pow(size, 6) * (1 - temp));

		gradient.x = gradientCoefficient * (position.x - other->position.x) * SmoothingKernel(other);
		gradient.y = gradientCoefficient * (position.x - other->position.x) * SmoothingKernel(other);
		gradient.z = gradientCoefficient * (position.x - other->position.x) * SmoothingKernel(other);
	}

	return gradient;
}

