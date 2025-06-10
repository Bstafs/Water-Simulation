#pragma once

#define PI 3.141592653589793238462643383279502884197

#include "Particle.h"
#include "SpatialGrid.h"

constexpr float dampingFactor = 0.99f;

constexpr  float minY = -30.0f, maxY = 50.0f;
constexpr  float minZ = -15.0f, maxZ = 15.0f;
constexpr  float minX = -50.0f, maxX = 50.0f;

struct ParticleAttributes
{
	XMFLOAT3 position; // 12 bytes
	float nearDensity; // 4 bytes (16-byte aligned)

	XMFLOAT3 velocity; // 12 bytes
	float density; // 4 bytes (16-byte aligned)
};

struct SimulationParams 
{
	int numParticles; // Total number of particles
	float deltaTime;
	float minX;
	float minZ;
};

struct BitonicParams
{
	unsigned int numElements;
	unsigned int k;
	unsigned int j;
	unsigned int padding;
};

class SPH
{
public:
	SPH(ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime, float minX, float minZ);


private:
	
	// Initial Particle Positions
	void InitParticles();
	void InitGPUResources();
	
	void UpdateSpatialGridClear(float deltaTime);
	void UpdateAddParticlesToSpatialGrid(float deltaTime);
	void UpdateBitonicSorting(float deltaTime);
	void UpdateBuildGridOffsets(float deltaTime);
	void UpdateParticleDensities(float deltaTime);
	void UpdateParticlePressure(float deltaTime);
	void UpdateIntegrateComputeShader(float deltaTime, float minX, float minZ);

	bool isBufferSwapped = false;

public:
	std::vector <Particle*> particleList;
private:
	// Particle Initialization

	ParticleAttributes* position;
	float mGravity = 0.0f;
	vector<XMFLOAT3> predictedPositions;

	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders
	ID3D11ComputeShader* FluidSimIntegrateShader = nullptr;
	ID3D11ComputeShader* SpatialGridClearShader = nullptr;
	ID3D11ComputeShader* SpatialGridAddParticleShader = nullptr;
	ID3D11ComputeShader* BitonicSortingShader = nullptr;
	ID3D11ComputeShader* GridOffsetsShader = nullptr;
	ID3D11ComputeShader* FluidSimCalculateDensity = nullptr;
	ID3D11ComputeShader* FluidSimCalculatePressure = nullptr;

	// Buffers
	ID3D11Buffer* inputBuffer = nullptr;
	ID3D11Buffer* outputBuffer = nullptr;
	ID3D11Buffer*  outputResultBuffer = nullptr;

	ID3D11Buffer*  SpatialGridConstantBuffer = nullptr;
	ID3D11Buffer*  BitonicSortConstantBuffer = nullptr;

	// Grid Buffer
	ID3D11Buffer* SpatialGridOutputBuffer = nullptr;
	ID3D11Buffer* SpatialGridResultOutputBuffer = nullptr;

	// Grid Count Buffer
	ID3D11Buffer* SpatialGridOutputBufferCount = nullptr;
	ID3D11Buffer* SpatialGridResultOutputBufferCount = nullptr;

	// Particle Positions SRV & UAV
	ID3D11ShaderResourceView* inputViewIntegrateA = nullptr;
	ID3D11UnorderedAccessView* outputUAVIntegrateA = nullptr;

	ID3D11ShaderResourceView* inputViewIntegrateB = nullptr;
	ID3D11UnorderedAccessView* outputUAVIntegrateB = nullptr;

	// Grid SRV & UAV
	ID3D11UnorderedAccessView* outputUAVSpatialGridA = nullptr;
	ID3D11UnorderedAccessView* outputUAVSpatialGridCountA = nullptr;

	ID3D11UnorderedAccessView* outputUAVSpatialGridB = nullptr;
	ID3D11UnorderedAccessView* outputUAVSpatialGridCountB = nullptr;

	ID3D11ShaderResourceView* srvNull[1] = { nullptr };
	ID3D11UnorderedAccessView* uavViewNull[1] = { nullptr };
};

