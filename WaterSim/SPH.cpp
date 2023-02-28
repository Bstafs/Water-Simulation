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


	float particleSpacing = 0.0045f;

	// Following Realtime Particle I researched, I need to loop over every particle for x,y,z

	const UINT iStartingWidth = (UINT)sqrt((FLOAT)numberOfParticles);

	for (int i = 0; i < numberOfParticles; ++i)
	{
		UINT x = i % iStartingWidth;
		UINT y = i % numberOfParticles;
		UINT z = i % iStartingWidth;

		XMFLOAT3 particlePosition = XMFLOAT3
		{
			particleSpacing * (FLOAT)x,
			particleSpacing * (FLOAT)y,
			particleSpacing * (FLOAT)z
		};

		Particle* newParticle = new Particle(MASS_CONSTANT, sphH, particlePosition, XMFLOAT3(1.0f, 1.0f, 1.0f));

		// Further Following Realtime Particle - Based Fluid Simulation, I add a random position to every particle and add it the the particle list.

		newParticle->elasticity = sphElasticity;
		newParticle->mass = MASS_CONSTANT;
		newParticle->pressure = 200.0f;
		newParticle->force = XMFLOAT3(1, 1, 1);
		newParticle->acceleration = XMFLOAT3(1, 1, 1);
		newParticle->density = 997.0f;

		particleList[i] = newParticle;
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
			part->position.y = -part->position.y + 2 * (part->size - collisionBoxSize);
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Bottom
		if (part->position.y > -part->size + collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * -(part->size - collisionBoxSize);
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Collision on the X Axis

		// Left
		if (part->position.x < part->size - collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * (part->size - collisionBoxSize);
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Right
		if (part->position.x > -part->size + collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * -(part->size - collisionBoxSize);
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Collision on the Z Axis

		// Back
		if (part->position.z < part->size - collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * (part->size - collisionBoxSize);
			part->velocity.z = -part->velocity.z * part->elasticity;
		}

		// Front
		if (part->position.z > -part->size + collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * -(part->size - collisionBoxSize);
			part->velocity.z = -part->velocity.z * part->elasticity;
		}
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


		//part->position.x = particlePositionValue.x + rand() % 100;
		//part->position.y = particlePositionValue.y + rand() % 100;
		//part->position.z = particlePositionValue.z + rand() % 100;

		//part->pressure = particlePressureValue;
		//part->density = particleDensityValue;
		//part->velocity = particleVelocityValue;

	}
}

void SPH::Update()
{
	//UpdateParticles();
	ParticleBoxCollision();
}

void SPH::Draw()
{

}