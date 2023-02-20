#include "SPH.h"

#define PI 3.14159265359f

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

	// A bit lost on this one todo figure this equation out.
	VISC_CONSTANT = 45.0f / (PI * pow(sphH, 6)) * (sphH - sphDensity);
	//	VISC_CONSTANT = 15.0f / (PI * pow(sphH, 6));

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

	//if (!neighbourParticles.empty())
	//{
	//	neighbourParticles.clear();

	//	for (int i = 0; i < neighbourParticles.size(); i++)
	//	{
	//		delete neighbourParticles[i][i];
	//		neighbourParticles[i][i] = nullptr;
	//	}
	//}

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
				float particleRandomPositionX = (float(rand()) / float((RAND_MAX)) * 0.5f - 1) * sphH / 10;
				float particleRandomPositionY = (float(rand()) / float((RAND_MAX)) * 0.5f - 1) * sphH / 10;
				float particleRandomPositionZ = (float(rand()) / float((RAND_MAX)) * 0.5f - 1) * sphH / 10;

				Vector3 particlePosition = Vector3
				{
					i * particleSpacing + particleRandomPositionX - 1.5f,
					j * particleSpacing + particleRandomPositionY + sphH + 0.1f,
					k * particleSpacing + particleRandomPositionZ - 1.5f
				};
				newParticle = new Particle(MASS_CONSTANT, sphH, particlePosition, Vector3(1.0f, 1.0f, 1.0f));

				// Further Following Realtime Particle - Based Fluid Simulation, I add a random position to every particle and add it the the particle list.
				newParticle->elasticity = sphElasticity;
				particleList[i + (j + numberOfParticles * k) * numberOfParticles] = newParticle;
			}
		}
	}
}

void CalculateDensity(const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); i++)
	{
		Particle* part = sph.particleList[i];
		// Density = mass * POLY6 / h^3
		float density = sph.MASS_CONSTANT * sph.POLY6_CONSTANT / pow(sph.sphH, 3);
		part->density += density;
	}
}

void CalculatePressure(const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); i++)
	{
		Particle* part = sph.particleList[i];

		part->density = sph.sphDensity + sph.DENS_CONSTANT;

		// p = k(density - rest density)
		float pressure = sph.GAS_CONSTANT * (part->density - sph.sphDensity);

		part->pressure += pressure;
	}
}

void CalculateForce(const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); i++)
	{
		Particle* part = sph.particleList[i];
		// Apply Viscosity and some other stuff
		part->force = Vector3(0, 0, 0);

		float distX = part->position.x;
		float distY = part->position.y;
		float distZ = part->position.z;
		float dist = distX + distY + distZ;

		Vector3 vosc;
		vosc.x = sph.sphViscosity * sph.MASS_CONSTANT * (part->velocity.x / part->density) * sph.SPIKY_CONSTANT * (sph.sphH - dist);
		vosc.y = sph.sphViscosity * sph.MASS_CONSTANT * (part->velocity.y / part->density) * sph.SPIKY_CONSTANT * (sph.sphH - dist);
		vosc.z = sph.sphViscosity * sph.MASS_CONSTANT * (part->velocity.z / part->density) * sph.SPIKY_CONSTANT * (sph.sphH - dist);
		part->force += vosc;

		// Dynamic Viscosity = Smoothing Length * Velocity Difference Between Particles * dot (separation distance) / (separation distance^2 + 0.01 * smoothing length^2)

		// Viscous Tensor = -viscosity * Speed of Sound * Dynamic Viscosity Coefficient + viscosity * speed of sound ^ 2 * density

	}
}

void ParticleBoxCollision(const SPH& sph)
{
	for (int i = 0; i < sph.particleList.size(); ++i)
	{
		Particle* part = sph.particleList[i];

		// Creating a box Collsion to hold particles. Continuining Following Realtime Particle - Based Fluid Simulation.
		float collisionBoxSize = 4.0f;

		// Collision on the y Axis
		if (part->position.y < part->size)
		{
			part->position.y = -part->position.y + 2 * part->size + 0.0001f;
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
	//	Vector3 acceleration = part->force / part->density;
		Vector3 acceleration = part->force / part->density + Vector3(0, sph.sphG, 0);
		part->velocity += acceleration * deltaTime;
		part->position += part->velocity * deltaTime;
		part = part->nextParticle;

	}
}

void SPH::Update(const SPH& sph, double deltaTime)
{
	CalculateDensity(sph);
	CalculatePressure(sph);
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