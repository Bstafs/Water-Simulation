#pragma once

// TO DO - Must
/* I need to grab particle Data to calculate each neighbouring particle to each other, maybe a vector?
 * Possibility need to learn how to make a hash table (std::unordered_map) for particles.
 * I need some fluid and kernel constants/variables for smoothing and external fluid sources
 *
 *
 */

 /* I will need a way to enter a particle amount, mass, density, gas considerations, viscosity, gravity and tension following
  * the research I have done.
  *
  *  I will need a function to calculate Density, Pressure, Force and one to update positions of particles.
  *
  *	 I will need a function to get each cell of a particle following matthias sph paper.
  *	 returning pos x,y,z / core radius?
  */

#include <unordered_map>
#include <vector>
#include "Particle.h"
#include "Structures.h"

class SPH
{
public:
	SPH(int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension, float elasticity);
	~SPH();
	void Update(const SPH& sph, double deltaTime);
	void Draw();
	// Particle Variables
	float sphViscosity;
	float sphDensity; // Rest Density of water is 997kg/m^3
	float sphTension; // 72 at room temp
	int numberOfParticles;
	float sphElasticity;

	// External force density field aka gravity
	float sphG;

	// Core Radius  aka Smoothing Length
	float sphH;

	// Fluid Constants
	float GAS_CONSTANT; // Needed for temperature 
	float MASS_CONSTANT;
	float H2_CONSTANT; // h^2
	float DENS_CONSTANT;

	// The 3 Kernel Smoothing Constants from Realtime Particle-Based Fluid Simulation
	float POLY6_CONSTANT;
	float SPIKY_CONSTANT;
	float SPIKYGRAD_CONSTANT;
	float VISC_CONSTANT;


	// Particle List
	std::vector <Particle*> particleList;
	Particle* newParticle;
	Vector3 GetPosition();

private:
	// Particle Initialization
	void InitParticles();
};

