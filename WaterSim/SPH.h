#pragma once

#define GRID_DIMENSION 256

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

using namespace Microsoft::WRL;


struct ParticleConstantBuffer
{
	int particleCount;
	float wallStiffness;
	XMFLOAT2 padding00;

	float deltaTime;
	float smoothingLength;
	float pressure;
	float restDensity;

	float densityCoef;
	float GradPressureCoef;
	float LapViscosityCoef;
	float gravity;

	XMFLOAT4 vPlanes[6];
	XMFLOAT4 gridDim;
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
};

struct GridBorderStructure
{
	UINT gridStart;
	UINT gridEnd;
};

class SPH
{
public:
	SPH(int numbParticles, float mass, float density, float viscosity, float h, float g, float elasticity, float pressure, ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update();
	void Draw();
	// Particle Variables
	float sphViscosity;
	float sphDensity; // Rest Density of water is 997kg/m^3
	int numberOfParticles;
	float sphElasticity;
	float sphPressure;

	// External force density field aka gravity
	float sphG;

	// Core Radius  aka Smoothing Length
	float sphH;

	// Fluid Constants
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

	float collisionBoxSize = 10.0f;

	ID3DUserDefinedAnnotation* _pAnnotation = nullptr;
private:
	// Particle Initialization

	void InitParticles();

	void ParticleBoxCollision();
	void SetUpParticleConstantBuffer();

	void BuildGrid();
	void SortGrid();
	void ClearGridIndices();
	void BuildGridIndices();
	void RearrangeParticles();
	void ParticleForcesSetup();

	void RenderFluid();

	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders

	ParticleConstantBuffer particleConstantCPUBuffer;
	SortConstantBuffer sortConstantCPUBuffer;

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11Buffer* pParticleConstantBuffer = nullptr;
	ID3D11Buffer* pSortConstantBuffer = nullptr;

	// Integrate
	ID3D11ComputeShader* pParticleIntegrateCS = nullptr;

	ID3D11Buffer* pIntegrateBufferOne = nullptr;
	ID3D11Buffer* pIntegrateBufferTwo = nullptr;

	ID3D11ShaderResourceView* pIntegrateSRVOne = nullptr;
	ID3D11ShaderResourceView* pIntegrateSRVTwo = nullptr;

	ID3D11UnorderedAccessView* pIntegrateUAVOne = nullptr;
	ID3D11UnorderedAccessView* pIntegrateUAVTwo = nullptr;

	bool bufferIsSwapped = false;

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

	ID3D11Buffer* pGridBufferTwo = nullptr;
	ID3D11ShaderResourceView* pGridSRVTwo = nullptr;
	ID3D11UnorderedAccessView* pGridUAVTwo = nullptr;

	bool isBufferSwappedGrid = false;

	// Grid Indices/Border
	ID3D11ComputeShader* pParticleGridIndicesCS = nullptr;

	ID3D11Buffer* pGridIndicesBufferOne = nullptr;
	ID3D11ShaderResourceView* pGridIndicesSRVOne = nullptr;
	ID3D11UnorderedAccessView* pGridIndicesUAVOne = nullptr;

	ID3D11Buffer* pGridIndicesBufferTwo = nullptr;
	ID3D11ShaderResourceView* pGridIndicesSRVTwo = nullptr;
	ID3D11UnorderedAccessView* pGridIndicesUAVTwo = nullptr;

	bool isBufferSwappedIndices = false;

	// Grid Sort Double Buffer
	ID3D11ComputeShader* pTransposeMatrixCS = nullptr;
	ID3D11Buffer* pGridIndicesTempBuffer = nullptr;
	ID3D11UnorderedAccessView* pGridIndicesTempUAV = nullptr;
	ID3D11ShaderResourceView* pGridIndicesTempSRV = nullptr;

	// Grid Sort
	ID3D11ComputeShader* pGridSorterShader = nullptr;
	//Clear Grid Indices
	ID3D11ComputeShader* pParticleClearGridIndicesCS = nullptr;

	// Rearrange Particles Shader
	ID3D11ComputeShader* pRearrangeParticlesCS = nullptr;

	//Debug
	ID3D11Buffer* pDebugDensityBuffer = nullptr;
	ID3D11Buffer* pDebugForceBuffer = nullptr;
	ID3D11Buffer* pDebugPositionBuffer = nullptr;
	ID3D11Buffer* pDebugGridBuffer = nullptr;
	ID3D11Buffer* pDebugGridIndicesBuffer = nullptr;

	ID3D11UnorderedAccessView* uavViewNull[1] = { nullptr };
	ID3D11ShaderResourceView* srvNull[2] = { nullptr, nullptr };
};

