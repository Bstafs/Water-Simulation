#pragma once

#define PI 3.141592653589793238462643383279502884197

#include "Particle.h"
#include "SpatialGrid.h"

struct ParticlePosition
{
	XMFLOAT3 positions;
};

class SPH
{
public:
	SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime);
	void UpdateSpatialGrid();

	// Particle Variables

	float sphGravity;

	XMFLOAT3 particlePositionValue;
	XMFLOAT3 particleVelocityValue;

	// Particle List
	std::vector <Particle*> particleList;

	float targetDensity = 4.0f;
	float stiffnessValue = 30.0f;

	float particleDensities[NUM_OF_PARTICLES];

	XMFLOAT3 halfBoundSize = XMFLOAT3(10.0f / 2 - 1.0f, 10.0f / 2 - 1.0f, 10.0f / 2 - 1.0f);

	float collisionBoxSize = 10.0f;

	float dt;

	ID3DUserDefinedAnnotation* _pAnnotation = nullptr;
private:
	// Particle Initialization
	SpatialGrid spatialGrid;

	void InitParticles();

	void SetUpParticleConstantBuffer();
	void ParticleForcesSetup();

	float CalculateMagnitude(const XMFLOAT3& vector);
	float* particleProperties;


	static float SmoothingKernel(float dst, float radius);
	static float SmoothingKernelDerivative(float radius, float dst);

	float CalculateDensity(const XMFLOAT3& samplePoint);
	float ConvertDensityToPressure(float density);
	XMFLOAT3 CalculatePressureForce(int particleIndex);
	float CalculateSharedPressure(float densityA, float densityB);
	XMFLOAT3 CalculatePressureForceWithRepulsion(int particleIndex);

	float* densities;

	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11ComputeShader* FluidSimComputeShader = nullptr;

	// Buffers
	ID3D11Buffer* positionBuffer = nullptr;
	ID3D11Buffer* velocityBuffer = nullptr;
	ID3D11Buffer* densityBuffer = nullptr;
	ID3D11Buffer* predictedPositionBuffer = nullptr;
	ID3D11Buffer* spatialIndicesBuffer = nullptr;
	ID3D11Buffer* spatialOffsetsBuffer = nullptr;

	// SRV & UAV
	ID3D11ShaderResourceView* pIntegrateSRVOne = nullptr;
	ID3D11UnorderedAccessView* pIntegrateUAVOne = nullptr;

	ID3D11UnorderedAccessView* uavViewNull[1] = { nullptr };
	ID3D11ShaderResourceView* srvNull[2] = { nullptr, nullptr };

	std::vector<XMFLOAT3> randomDirections; // Cached random directions
	int numRandomDirections = 100;          // Adjust as needed

	void InitRandomDirections();
	XMFLOAT3 GetRandomDir();
};

