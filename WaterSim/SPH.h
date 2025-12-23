#pragma once

#define PI 3.141592653589793238462643383279502884197

#include "Particle.h"
#include "MarchingCubes.h"

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

struct GridIndexGPU { // 16 bytes
	UINT particleIndex;
	UINT hash;
	UINT key;
	UINT pad; // pad to 16 bytes
};

struct Voxel
{
	float density;
	XMFLOAT3 padding;
};

struct MCGridParams
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
	float voxelSize;
};

class SPH
{
public:
	SPH(ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime, float minX, float minZ);

	ID3D11ShaderResourceView* GetParticlePositionSRV() const { return g_pParticlePositionSRV; }
	float GetVoxelCount() const { return VOXEL_COUNT; }

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


	void UpdateMarchingCubes();

	bool isBufferSwapped = false;

public:
	std::vector <Particle> particleList;
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

	ID3D11ComputeShader* MarchingCubesShader = nullptr;

	// Buffers
	ID3D11Buffer* inputBuffer = nullptr;
	ID3D11Buffer* outputBuffer = nullptr;
	ID3D11Buffer*  outputResultBuffer = nullptr;

	ID3D11Buffer* g_pParticlePositionBuffer = nullptr;
	ID3D11ShaderResourceView* g_pParticlePositionSRV = nullptr;
	ID3D11UnorderedAccessView* g_pParticlePositionUAV = nullptr;

	// Constant Buffers
	ID3D11Buffer*  SpatialGridConstantBuffer = nullptr;
	ID3D11Buffer*  BitonicSortConstantBuffer = nullptr;

	// Grid Buffer
	ID3D11Buffer* SpatialGridOutputBufferA = nullptr;
	ID3D11Buffer* SpatialGridOutputBufferB = nullptr;

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

	ID3D11ShaderResourceView* outputSRVSpatialGridA = nullptr;
	ID3D11ShaderResourceView* outputSRVSpatialGridB = nullptr;

	ID3D11ShaderResourceView* srvNull[1] = { nullptr };
	ID3D11UnorderedAccessView* uavViewNull[1] = { nullptr };

	// Marchine	Cubes 
	ID3D11Buffer* voxelBuffer = nullptr;
	ID3D11UnorderedAccessView* voxelUAV = nullptr;
	ID3D11ShaderResourceView* voxelSRV = nullptr;

	ID3D11Buffer* MCConstantBuffer = nullptr;

	float worldMinX = -50;
	float worldMaxX = 50;

	float worldMinY = -30;
	float worldMaxY = 50;

	float worldMinZ = -50;
	float worldMaxZ = 50;

	float voxelSize = 1.25f;

	int gridSizeX = (worldMaxX - worldMinX) / voxelSize;
	int gridSizeY = (worldMaxY - worldMinY) / voxelSize;
	int gridSizeZ = (worldMaxZ - worldMinZ) / voxelSize;

	int VOXEL_COUNT = gridSizeX * gridSizeY * gridSizeZ;

	ID3DUserDefinedAnnotation* g_Annotation = nullptr;
};

