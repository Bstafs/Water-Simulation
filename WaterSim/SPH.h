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

#include "Particle.h"
#include "Includes.h"

struct ParticleConstantBuffer
{
	int particleCount;
	XMFLOAT3 padding00;

	float deltaTime;
	float smoothingLength;
	float pressure;
	float restDensity;

	float densityCoef;
	float GradPressureCoef;
	float LapViscosityCoef;
	float gravity;

	XMFLOAT4 vPlanes[6];
};

struct SortConstantBuffer
{
	unsigned int sortLevel;
	unsigned int sortAlternateMask;
	unsigned int iWidth;
	unsigned int iHeight;

	XMFLOAT4 padding01;
};

struct IntegrateParticle
{
	XMFLOAT3 position;
	float padding01;

	XMFLOAT3 velocity;
	float padding02;
};

struct ParticleForces
{
	XMFLOAT3 acceleration;
	float padding01;
};

struct ParticleDensity
{
	XMFLOAT3 padding01;
	float density;
};

struct GridKeyStructure
{
	UINT gridKey;
	UINT particleIndex;
};

struct GridBorderStructure
{
	UINT gridStart;
	UINT gridEnd;
};

class SPH
{
public:
	SPH(int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension, float elasticity, float pressure, ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update();
	void Draw();
	// Particle Variables
	float sphViscosity;
	float sphDensity; // Rest Density of water is 997kg/m^3
	float sphTension; // 72 at room temp
	int numberOfParticles;
	float sphElasticity;
	float sphPressure;

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

	XMFLOAT3 particlePositionValue;
	XMFLOAT3 particleVelocityValue;
	float particlePressureValue;
	float particleDensityValue;
	float deltaTimeValue;

	// Particle List
	std::vector <Particle*> particleList;

	float collisionBoxSize = 1.0f;
private:
	// Particle Initialization
	void InitParticles();
	void ParticleBoxCollision();
	void ParticleForcesSetup();
	void SetUpParticleConstantBuffer();
	void BuildGrid();
	void BuildGridIndices();
	void SortGridIndices();
	void RenderFluid();

	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders

	ParticleConstantBuffer particleConstantCPUBuffer;

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11Buffer* pParticleConstantBuffer = nullptr;
	ID3D11Buffer* pSortConstantBuffer = nullptr;

	// Integrate
	ID3D11ComputeShader* pParticleIntegrateCS = nullptr;
	ID3D11Buffer* pIntegrateBuffer = nullptr;
	ID3D11ShaderResourceView* pIntegrateSRV = nullptr;
	ID3D11UnorderedAccessView* pIntegrateUAV = nullptr;

	// Forces
	ID3D11ComputeShader* pParticleForcesCS = nullptr;
	ID3D11Buffer* pForcesBuffer = nullptr;
	ID3D11ShaderResourceView* pForcesSRV = nullptr;
	ID3D11UnorderedAccessView* pForcesUAV = nullptr;

	// Density
	ID3D11ComputeShader* pParticleDensityCS = nullptr;
	ID3D11Buffer* pDensityBuffer = nullptr;
	ID3D11ShaderResourceView* pDensitySRV = nullptr;
	ID3D11UnorderedAccessView* pDensityUAV = nullptr;

	// Grid
	ID3D11ComputeShader* pParticleGridCS = nullptr;
	ID3D11Buffer* pGridBuffer = nullptr;
	ID3D11ShaderResourceView* pGridSRV = nullptr;
	ID3D11UnorderedAccessView* pGridUAV = nullptr;

	// Grid Indices/Border
	ID3D11ComputeShader* pParticleGridIndicesCS = nullptr;
	ID3D11Buffer* pGridIndicesBuffer = nullptr;
	ID3D11ShaderResourceView* pGridIndicesSRV = nullptr;
	ID3D11UnorderedAccessView* pGridIndicesUAV = nullptr;

	// Grid Sort
	ID3D11ComputeShader* pGridSorterShader = nullptr;
	//Clear Grid Indices
	ID3D11ComputeShader* pParticleClearGridIndicesCS = nullptr;

	//Debug
	ID3D11Buffer* pDebugDensityBuffer = nullptr;
	ID3D11Buffer* pDebugForceBuffer = nullptr;
	ID3D11Buffer* pDebugPositionBuffer = nullptr;
	ID3D11Buffer* pDebugGridBuffer = nullptr;
	ID3D11Buffer* pDebugGridIndicesBuffer = nullptr;
};

