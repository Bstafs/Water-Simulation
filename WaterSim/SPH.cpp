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
	int particleResize = numberOfParticles * numberOfParticles * numberOfParticles;
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
		particleList.shrink_to_fit();
		for (auto particlesL : particleList)
		{
			delete particlesL;
			particlesL = nullptr;
		}
	}

	delete newParticle;
	newParticle = nullptr;
}

void SPH::InitParticles()
{
	/* Loop through x, y, z number of particles
	 * Calculate a Vector3 Particle Pos using randomized positions
	 * Create a new Particle Base on that position
	 * Add the particles to the particle list
	 */

	std::srand(1024);

	float particleSpacing = sphH + 0.01f;

	// Following Realtime Particle I researched, I need to loop over every particle for x,y,z

	for (int i = 0; i < numberOfParticles; ++i)
	{
		for (int j = 0; j < numberOfParticles; ++j)
		{
			for (int k = 0; k < numberOfParticles; ++k)
			{
				float particleRandomPositionX = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH / 10;
				float particleRandomPositionY = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH / 10;
				float particleRandomPositionZ = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH / 10;

				XMFLOAT3 particlePosition = XMFLOAT3
				{
					i * particleSpacing + particleRandomPositionX - 1.5f,
					j * particleSpacing + particleRandomPositionY + sphH + 0.1f,
					k * particleSpacing + particleRandomPositionZ - 1.5f
				};

				newParticle = new Particle(MASS_CONSTANT, sphH, particlePosition, XMFLOAT3(1.0f, 1.0f, 1.0f));

				// Further Following Realtime Particle - Based Fluid Simulation, I add a random position to every particle and add it the the particle list.
				particleList[i + (j + numberOfParticles * k) * numberOfParticles] = newParticle;
			}
		}
	}
}

void SPH::CalculateDensityPressureMass()
{
	Particle* particleOne = nullptr;

	for (int i = 0; i < particleList.size(); i++)
	{
		particleOne = particleList[i];

		particleOne->SetDensity(tempDensityValue + DENS_CONSTANT);

		float pressure = GAS_CONSTANT * (particleOne->density - tempPressureValue);
		particleOne->SetPressure(pressure);

		float vol = 4.0f / 3.0f * PI * pow(sphH, 3);
		particleOne->SetMass(MASS_CONSTANT);
	}
}

void SPH::CalculateForce()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* particleOne = particleList[i];

		particleOne->force.x += tempForceValue.x;
		particleOne->force.y += tempForceValue.y;
		particleOne->force.z += tempForceValue.z;
	}
}

void SPH::ParticleBoxCollision()
{
	for (int i = 0; i < particleList.size(); ++i)
	{
		Particle* part = particleList[i];

		// Creating a box Collsion to hold particles. Continuining Following Realtime Particle - Based Fluid Simulation.
		float collisionBoxSize = 5.0f;

		// Collision on the y Axis
		if (part->position.y < part->size - collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * (part->size - collisionBoxSize) + 0.0001f;
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		if (part->position.y > -part->size + collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * -(part->size - collisionBoxSize) - 0.0001f;
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Collision on the X Axis
		if (part->position.x < part->size - collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * (part->size - collisionBoxSize) + 0.0001f;
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		if (part->position.x > -part->size + collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * -(part->size - collisionBoxSize) - 0.0001f;
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Collision on the Z Axis
		if (part->position.z < part->size - collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * (part->size - collisionBoxSize) + 0.0001f;
			part->velocity.z = -part->velocity.z * part->elasticity;
		}

		if (part->position.z > -part->size + collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * -(part->size - collisionBoxSize) - 0.0001f;
			part->velocity.z = -part->velocity.z * part->elasticity;
		}
	}
}

void SPH::UpdateParticles()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* part = particleList[i];

		part->position.x += tempPositionValue.x * MASS_CONSTANT;
		part->position.y += tempPositionValue.y * MASS_CONSTANT;
		part->position.z += tempPositionValue.z * MASS_CONSTANT;
	}
}

void SPH::Update(const SPH& sph, double deltaTime)
{
	CalculateDensityPressureMass();
	CalculateForce();
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