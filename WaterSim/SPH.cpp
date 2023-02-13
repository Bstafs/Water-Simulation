#include "SPH.h"

#define PI 3.14159265359f

SPH::SPH(unsigned int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension)
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
	VISC_CONSTANT = 15.0f / (2 * PI * pow(sphH, 3));

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
	initParticles();


}

SPH::~SPH()
{

}

void SPH::Update(double deltaTime)
{

}

void SPH::initParticles()
{

}
