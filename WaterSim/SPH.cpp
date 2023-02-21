#include "SPH.h"

// Speed of Sound in Water
float SOS_CONSTANT = 1480.0f;

SPH::SPH(int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension, float elasticity)
{
	// Variable Initialization
	numberOfParticles = numbParticles;
	sphDensity = density;
	sphViscosity = viscosity;
	sphH = h;
	sphG = g;
	sphTension = tension;
	sphElasticity = elasticity;

	// Kernel Smoothing Constants Initialization
	POLY6_CONSTANT = 315.0f / (64.0f * PI * pow(sphH, 9));

	SPIKY_CONSTANT = 15.0f / (PI * pow(sphH, 6));
	SPIKYGRAD_CONSTANT = -15.0f / (PI * pow(h, 6));

	VISC_CONSTANT = 15.0f / (PI * pow(sphH, 6));

	// Particle Constant Initialization
	MASS_CONSTANT = mass;
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

				Vector3 particlePosition = Vector3
				{
					i * particleSpacing + particleRandomPositionX - 1.5f,
					j * particleSpacing + particleRandomPositionY + sphH + 0.1f,
					k * particleSpacing + particleRandomPositionZ - 1.5f
				};

				MASS_CONSTANT = (sphDensity * (4.0f / 3.0f * PI * pow(sphH, 3))) + DENS_CONSTANT;

				newParticle = new Particle(MASS_CONSTANT, sphH, particlePosition, Vector3(1.0f, 1.0f, 1.0f));

				newParticle->elasticity = sphElasticity;

				// Further Following Realtime Particle - Based Fluid Simulation, I add a random position to every particle and add it the the particle list.
				particleList[i + (j + numberOfParticles * k) * numberOfParticles] = newParticle;
			}
		}
	}
}

void CalculateDensityPressureMass(const SPH& sph)
{
	float massPoly6 = sph.MASS_CONSTANT * sph.POLY6_CONSTANT;

	for (int i = 0; i < sph.particleList.size(); i++)
	{
		Particle* particleOne = sph.particleList[i];

		float tempDensity = 0.0f;

		for (int j = 0; j < sph.particleList.size(); j++)
		{
			Particle* particleTwo = sph.particleList[j];

			float dist = particleOne->Distance(particleTwo);

			tempDensity += massPoly6 * pow(sph.H2_CONSTANT - dist, 3);
		}
		particleOne->SetDensity(tempDensity + sph.DENS_CONSTANT);

		float pressure = sph.GAS_CONSTANT * (particleOne->density - sph.sphDensity);
		particleOne->SetPressure(pressure);

		float vol = 4.0f / 3.0f * PI * pow(sph.sphH, 3);
		particleOne->SetMass(sph.sphDensity * vol);
	}
}

// Dynamic Viscosity Coefficient = Smoothing Length * Velocity Difference Between Particles * dot product (separation distance) / (separation distance^2 + 0.01 * smoothing length^2)
// Viscous Tensor = -viscosity * Speed of Sound * Dynamic Viscosity Coefficient + viscosity * speed of sound ^ 2 * density

void CalculateForce(const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); i++)
	{
		Particle* particleOne = sph.particleList[i];

		particleOne->force = { 0,0,0 };

		for (int j = 0; j < sph.particleList.size(); j++)
		{
			Particle* particleTwo = sph.particleList[j];

			// Check if each particle is next to each other or not
			// We Grab the distance between the two particles
			float dist2 = particleOne->Distance(particleTwo);

			if (dist2 < sph.sphH && particleOne != particleTwo)
			{
				float dist = sqrt(dist2);
				Vector3 dir = particleTwo->position - particleOne->position;
				dir.Normalize();

				// Calculate Pressure Force
				Vector3 pressureForce = { 0,0,0 };

				// Test 1
				float particlePressureCoefficient = -particleOne->mass * particleTwo->mass / (2.0f * particleTwo->density) *sph.SPIKY_CONSTANT;

				Vector3 gradientKernel = particleOne->GradientKernel(particleTwo);

				pressureForce.x += particlePressureCoefficient * gradientKernel.x;
				pressureForce.y += particlePressureCoefficient * gradientKernel.y;
				pressureForce.z += particlePressureCoefficient * gradientKernel.z;

				// Test 2
				//pressureForce = (dir * -1.0f) * sph.MASS_CONSTANT * (particleOne->pressure + particleTwo->pressure) / (2 * particleTwo->density) * sph.SPIKYGRAD_CONSTANT;
				//pressureForce *= pow(sph.sphH - dist, 2);

				particleOne->force += pressureForce;

				// Calculate Viscosity Force
				Vector3 viscosityForce = { 0,0,0 };

				// Test 1
				float particleViscosityCoefficient = sph.sphViscosity * particleOne->mass * particleTwo->mass / (particleTwo->mass * (particleOne->density + particleTwo->density));

				Vector3 velocityDifference = particleTwo->velocity - particleOne->velocity;

				viscosityForce.x += particleViscosityCoefficient * velocityDifference.x;
				viscosityForce.y += particleViscosityCoefficient * velocityDifference.y;
				viscosityForce.z += particleViscosityCoefficient * velocityDifference.z;

				// Test 2
				//viscosityForce.x = sph.sphViscosity * sph.MASS_CONSTANT * (velocityDifference.x / particleTwo->density) * sph.SPIKY_CONSTANT * (sph.sphH - dist);
				//viscosityForce.y = sph.sphViscosity * sph.MASS_CONSTANT * (velocityDifference.y / particleTwo->density) * sph.SPIKY_CONSTANT * (sph.sphH - dist);
				//viscosityForce.z = sph.sphViscosity * sph.MASS_CONSTANT * (velocityDifference.z / particleTwo->density) * sph.SPIKY_CONSTANT * (sph.sphH - dist);
				
				particleOne->force += viscosityForce;
			}
		}
	}
}

void ParticleBoxCollision(const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); ++i)
	{
		Particle* part = sph.particleList[i];

		// Creating a box Collsion to hold particles. Continuining Following Realtime Particle - Based Fluid Simulation.
		float collisionBoxSize = 10.0f;

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

void UpdateParticles(double deltaTime, const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); i++)
	{
		Particle* part = sph.particleList[i];

		// acceleration = particle force / particle mass
		//Vector3 acceleration = part->force / part->density;
		Vector3 acceleration = part->force / part->density /*+ Vector3(0, sph.sphG, 0)*/;
		part->velocity += acceleration * deltaTime;
		part->position += part->velocity * deltaTime;
	}
}

void SPH::Update(const SPH& sph, double deltaTime)
{
	CalculateDensityPressureMass(sph);
	CalculateForce(sph);
	UpdateParticles(deltaTime, sph);
	ParticleBoxCollision(sph);
}

Vector3 SPH::GetPosition()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		return particleList[i]->position;
	}
}

void SPH::Draw()
{

}