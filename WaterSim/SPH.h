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
  *
  */

#include <unordered_map>
#include <vector>
#include "Particle.h"

class SPH
{
public:
	SPH(unsigned int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension);
	~SPH();

	void Update(double deltaTime);

	// Particle Variables
	float sphViscosity;
	float sphDensity;
	float sphTension;

	// External force density field 
	float sphG;

	// Core Radius 
	float sphH;

	// Fluid Constants
	float GAS_CONSTANT; // Needed for temperature 
	float MASS_CONSTANT;
	float H2_CONSTANT; // h^2
	float DENS_CONSTANT;

	// The 3 Kernel Smoothing Constants from Realtime Particle-Based Fluid Simulation
	float POLY6_CONSTANT;
	float SPIKY_CONSTANT;
	float VISC_CONSTANT;


	// Particle List
	std::vector <Particle*> particleList;

private:
	// Particle Initialization
	void initParticles();

	// Particle Data
	unsigned int numberOfParticles;

	// 2D vector to traverse and access each particle to check each neighbour
	// https://www.geeksforgeeks.org/vector-of-vectors-in-c-stl-with-examples/ - Current Resource 
	std::vector<std::vector<Particle*>> neighbourParticles;

	// TO DO: not sure what to do yet, will find other ideas later
	std::unordered_map<Particle*, Particle*> particleLinkedListMap;
};

