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

struct GridIndexGPU { // 16 bytes
	UINT particleIndex;
	UINT hash;
	UINT key;
	UINT pad; // pad to 16 bytes
};

struct RadixParams
{
	UINT numElements;
	UINT bitShift;
	UINT groupCount;
	UINT radixPadding;
};

class SPH
{
public:
	SPH(ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime, float minX, float minZ);

	ID3D11ShaderResourceView* GetParticlePositionSRV() const { return g_pParticlePositionSRV; }

private:
	
	// Initial Particle Positions
	void InitParticles();
	void InitGPUResources();
	
	void UpdateSpatialGridClear(float deltaTime);
	void UpdateAddParticlesToSpatialGrid(float deltaTime);
	void UpdateRadixSorting(float deltaTime);
	void UpdateBuildGridOffsets(float deltaTime);
	void UpdateParticleDensities(float deltaTime);
	void UpdateParticlePressure(float deltaTime);
	void UpdateIntegrateComputeShader(float deltaTime, float minX, float minZ);

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

	ID3D11ComputeShader* GridOffsetsShader = nullptr;
	ID3D11ComputeShader* FluidSimCalculateDensity = nullptr;
	ID3D11ComputeShader* FluidSimCalculatePressure = nullptr;

	// Buffers
	ID3D11Buffer* inputBuffer = nullptr;
	ID3D11Buffer* outputBuffer = nullptr;
	ID3D11Buffer*  outputResultBuffer = nullptr;

	ID3D11Buffer* g_pParticlePositionBuffer = nullptr;
	ID3D11ShaderResourceView* g_pParticlePositionSRV = nullptr;
	ID3D11UnorderedAccessView* g_pParticlePositionUAV = nullptr;

	// Constant Buffers
	ID3D11Buffer*  SpatialGridConstantBuffer = nullptr;

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

	ID3DUserDefinedAnnotation* g_Annotation = nullptr;


	// RADIX 
	static constexpr UINT RADIX_BITS = 4;
	static constexpr UINT RADIX_SIZE = 1u << RADIX_BITS; // 16
	static constexpr UINT RADIX_MASK = RADIX_SIZE - 1;   // 15
	static constexpr UINT RADIX_THREADS = 256;

	UINT groupCount =
		(NUM_OF_PARTICLES + RADIX_THREADS - 1) / RADIX_THREADS;

	ID3D11ComputeShader* RadixHistogramShader = nullptr;
	ID3D11ComputeShader* RadixPrefixShader = nullptr;
	ID3D11ComputeShader* RadixScatterShader = nullptr;

	ID3D11Buffer* RadixConstantBuffer = nullptr;
	ID3D11Buffer* RadixHistogramBuffer = nullptr;
	ID3D11Buffer* RadixPrefixBuffer = nullptr;
	ID3D11Buffer* SpatialGridScratchBuffer = nullptr;

	ID3D11UnorderedAccessView* RadixHistogramUAV = nullptr;
	ID3D11UnorderedAccessView* RadixPrefixUAV = nullptr;

	ID3D11UnorderedAccessView* SpatialGridScratchUAV = nullptr;
	ID3D11ShaderResourceView* SpatialGridScratchSRV = nullptr;

};

