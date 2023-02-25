#include "SPH.h"

// Speed of Sound in Water
float SOS_CONSTANT = 1480.0f;

SPH::SPH(int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension, float elasticity, float pressure)
{
	// Variable Initialization
	numberOfParticles = numbParticles;
	sphDensity = density;
	sphViscosity = viscosity;
	sphH = h;
	sphG = g;
	sphTension = tension;
	sphElasticity = elasticity;
	sphPressure = pressure;

	MASS_CONSTANT = mass;

	// Kernel Smoothing Constants Initialization
	POLY6_CONSTANT = MASS_CONSTANT * 315.0f / (64.0f * PI * powf(sphH, 9));

	SPIKY_CONSTANT = MASS_CONSTANT * 45.0f / (PI * powf(sphH, 6));
	SPIKYGRAD_CONSTANT = MASS_CONSTANT * -45.0f / (PI * powf(sphH, 6));

	VISC_CONSTANT = MASS_CONSTANT * sphViscosity * 45.0f / (PI * powf(sphH, 6));

	// Particle Constant Initialization

	GAS_CONSTANT = gasConstant;
	H2_CONSTANT = sphH * sphH;
	DENS_CONSTANT = MASS_CONSTANT * POLY6_CONSTANT * pow(sphH, 6);

	// Particle Resize - Since I'm Going to be looping through the particles 3 times for the x,y,z position.
	int particleResize = numberOfParticles;
	//	neighbourParticles.resize(particleResize);
	particleList.resize(particleResize);

	// Particle Initialization
	InitParticles();
}

SPH::~SPH()
{
	// Particle Cleanup
	if (!particleList.empty())
	{
		particleList.clear();
		for (auto particlesL : particleList)
		{
			delete particlesL;
			particlesL = nullptr;
		}
	}
}

void SPH::InitParticles()
{
	/* Loop through x, y, z number of particles
	 * Calculate a Vector3 Particle Pos using randomized positions
	 * Create a new Particle Base on that position
	 * Add the particles to the particle list
	 */


	float particleSpacing = sphH + 0.01f;

	// Following Realtime Particle I researched, I need to loop over every particle for x,y,z

	for (int i = 0; i < numberOfParticles; ++i)
	{
		//float particleRandomPositionX = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH;
		//float particleRandomPositionY = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH;
		//float particleRandomPositionZ = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH;

		int maxiumumSpawnRadius = collisionBoxSize - 1.5f;

		int particleRandomPositionX = rand() % maxiumumSpawnRadius;
		int particleRandomPositionY = rand() % maxiumumSpawnRadius;
		int particleRandomPositionZ = rand() % maxiumumSpawnRadius;

		XMFLOAT3 particlePosition = XMFLOAT3
		{
			particleSpacing + particleRandomPositionX - 1.5f,
			particleSpacing + particleRandomPositionY + sphH + 0.1f,
			particleSpacing + particleRandomPositionZ - 1.5f
		};

		Particle* newParticle = new Particle(MASS_CONSTANT, sphH, particlePosition, XMFLOAT3(1.0f, 1.0f, 1.0f));

		// Further Following Realtime Particle - Based Fluid Simulation, I add a random position to every particle and add it the the particle list.

		particleList[i] = newParticle;

		newParticle->elasticity = sphElasticity;
		newParticle->mass = MASS_CONSTANT;
	}
}

void SPH::ParticleBoxCollision()
{
	for (int i = 0; i < particleList.size(); ++i)
	{
		Particle* part = particleList[i];

		// Creating a box Collsion to hold particles. Continuining Following Realtime Particle - Based Fluid Simulation.

		// Collision on the y Axis

		// Top
		if (part->position.y < part->size - collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * (part->size - collisionBoxSize) + 0.0001f;
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Bottom
		if (part->position.y > -part->size + collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * -(part->size - collisionBoxSize) - 0.0001f;
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Collision on the X Axis

		// Left
		if (part->position.x < part->size - collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * (part->size - collisionBoxSize) + 0.0001f;
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Right
		if (part->position.x > -part->size + collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * -(part->size - collisionBoxSize) - 0.0001f;
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Collision on the Z Axis

		// Back
		if (part->position.z < part->size - collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * (part->size - collisionBoxSize) + 0.0001f;
			part->velocity.z = -part->velocity.z * part->elasticity;
		}

		// Front
		if (part->position.z > -part->size + collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * -(part->size - collisionBoxSize) - 0.0001f;
			part->velocity.z = -part->velocity.z * part->elasticity;
		}

		//// Particle Collision
		//for(int j =0; j < particleList.size(); j++)
		//{
		//	Particle* part2 = particleList[j];

		//	// Top
		//	if (part->position.y < part2->position.y)
		//	{
		//		part->position.y = -part->position.y + 2;
		//		part->velocity.y = -part->velocity.y * part->elasticity;
		//	}

		//	// Bottom
		//	if (part->position.y > -part2->position.y)
		//	{
		//		part->position.y = -part->position.y + 2;
		//		part->velocity.y = -part->velocity.y * part->elasticity;
		//	}

		//	// Collision on the X Axis

		//	// Left
		//	if (part->position.x < part2->position.x)
		//	{
		//		part->position.x = -part->position.x + 2;
		//		part->velocity.x = -part->velocity.x * part->elasticity;
		//	}

		//	// Right
		//	if (part->position.x > -part2->position.x)
		//	{
		//		part->position.x = -part->position.x + 2;
		//		part->velocity.x = -part->velocity.x * part->elasticity;
		//	}

		//	// Collision on the Z Axis

		//	// Back
		//	if (part->position.z < part2->position.z)
		//	{
		//		part->position.z = -part->position.z + 2;
		//		part->velocity.z = -part->velocity.z * part->elasticity;
		//	}

		//	// Front
		//	if (part->position.z > -part2->position.z)
		//	{
		//		part->position.z = -part->position.z + 2;
		//		part->velocity.z = -part->velocity.z * part->elasticity;
		//	}
		//}
	}
}

void SPH::UpdateParticles()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* part = particleList[i];


		//part->position.x = particlePositionValue.x;
		//part->position.y = particlePositionValue.y;
		//part->position.z = particlePositionValue.z;


		part->position.x = particlePositionValue.x + rand() % 100;
		part->position.y = particlePositionValue.y + rand() % 100;
		part->position.z = particlePositionValue.z + rand() % 100;

		part->pressure = particlePressureValue;
		part->density = particleDensityValue;
		part->velocity = particleVelocityValue;

	}
}

void SPH::Update()
{
	UpdateParticles();
	ParticleBoxCollision();
}

XMFLOAT3 SPH::GetPosition()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		return particleList[i]->position;
	}
}

XMFLOAT3 SPH::GetVelocity()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		return particleList[i]->velocity;
	}
}

XMFLOAT3 SPH::GetForce()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		return particleList[i]->force;
	}
}

XMFLOAT3 SPH::GetAccel()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		return particleList[i]->acceleration;
	}
}


void SPH::Draw()
{

}