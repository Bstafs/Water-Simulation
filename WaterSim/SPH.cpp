#include "SPH.h"

#define PI 3.14159265359f

SPH::SPH(int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension)
{
	// Variable Initialization
	numberOfParticles = numbParticles;
	sphDensity = density;
	sphViscosity = viscosity;
	sphH = h;
	sphG = g;
	sphTension = tension;

	// Kernel Smoothing Constants Initialization
	POLY6_CONSTANT = 315.0f / (64.0f * PI * pow(sphH, 9));
	SPIKY_CONSTANT = 15.0f / (PI * pow(sphH, 6));
	VISC_CONSTANT = 45.0f / (PI * pow(sphH, 6)) * (sphH - sphDensity);

	// Particle Constant Initialization
	MASS_CONSTANT = mass;
	GAS_CONSTANT = gasConstant;
	H2_CONSTANT = sphH * sphH;
	DENS_CONSTANT = MASS_CONSTANT * POLY6_CONSTANT * pow(sphH, 6);

	// Particle Resize
	int particleResize = numberOfParticles * numberOfParticles * numberOfParticles;
	neighbourParticles.resize(particleResize);
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

	if (!neighbourParticles.empty())
	{
		neighbourParticles.clear();
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

	std::srand(512);

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

				Vector3 particlePosition = Vector3{ i * particleSpacing + particleRandomPositionX - 1.5f, j * particleSpacing + particleRandomPositionY + sphH + 0.1f, k * particleSpacing + particleRandomPositionZ - 1.5f };
				newParticle = new Particle(MASS_CONSTANT, sphH, particlePosition, Vector3(0, 0, 0));

				// Further Following Realtime Particle - Based Fluid Simulation, I add a random position to every particle and add it the the particle list.
				particleList[i + (j + numberOfParticles * k) * numberOfParticles] = newParticle;
			}
		}
	}
}

void SPH::CalculateDensity()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* part = particleList[i];
		float density = MASS_CONSTANT * POLY6_CONSTANT / pow(sphH, 3);
		part->density = density;
	}
}

void SPH::CalculatePressure()
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* part = particleList[i];

		part->density = sphDensity + DENS_CONSTANT;

		// p = k(density - rest density)
		float pressure = GAS_CONSTANT * (part->density - sphDensity);

		part->pressure = pressure;
	}
}

void SPH::CalculateForce(double deltaTime)
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* part = particleList[i];



	}
}

void SPH::UpdateParticles(double deltaTime)
{
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* part = particleList[i];

		// acceleration = particle force / particle density
		Vector3 acceleration = part->force / part->density;
		part->velocity += acceleration * deltaTime;
		part->position += part->velocity * deltaTime;
	}
}

void SPH::Update(const SPH& sph, double deltaTime)
{
	CalculateDensity();
	CalculatePressure();
	CalculateForce(deltaTime);
	UpdateParticles(deltaTime);
}

void SPH::Draw()
{

}