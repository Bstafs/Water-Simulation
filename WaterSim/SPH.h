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

struct RadixParams
{
	unsigned int particleCount;
	unsigned int numGroups;        
	unsigned int currentBit;        
	unsigned int pad0;
};

struct GridIndexGPU { // 16 bytes
	UINT particleIndex;
	UINT hash;
	UINT key;
	UINT pad; // pad to 16 bytes
};

class SPH
{
public:
	SPH(ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime, float minX, float minZ);

	ID3D11Buffer* GetMarchingCubesVertexBuffer() const { return _pVertexBufferMarchingCubes; }
	ID3D11Buffer* GetMarchingCubesIndexBuffer() const { return _pIndexBufferMarchingCubes; }
	UINT GetMarchingCubesIndexCount() const { return static_cast<UINT>(MarchingCubesIndices.size()); }

	ID3D11ShaderResourceView* GetParticlePositionSRV() const { return g_pParticlePositionSRV; }

private:
	
	// Initial Particle Positions
	void InitParticles();
	void InitGPUResources();
	
	void UpdateSpatialGridClear(float deltaTime);
	void UpdateAddParticlesToSpatialGrid(float deltaTime);
	void UpdateBitonicSorting(float deltaTime);

	void UpdateRadixSorting(float deltaTime);

	void UpdateBuildGridOffsets(float deltaTime);
	void UpdateParticleDensities(float deltaTime);
	void UpdateParticlePressure(float deltaTime);
	void UpdateIntegrateComputeShader(float deltaTime, float minX, float minZ);

	void UpdateMarchingCubes(float deltaTime);
	XMFLOAT3 CalculateParticleCenter();
	void UpdateGridOrigin();
	void UpdateMarchingCubesBuffers();


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

	ID3D11ComputeShader* RadixHistogramShader = nullptr;
	ID3D11ComputeShader* RadixPreFixScanShader = nullptr;
	ID3D11ComputeShader* RadixPrepareOffsetsShader = nullptr;
	ID3D11ComputeShader* RadixScatterShader = nullptr;

	ID3D11ComputeShader* GridOffsetsShader = nullptr;
	ID3D11ComputeShader* FluidSimCalculateDensity = nullptr;
	ID3D11ComputeShader* FluidSimCalculatePressure = nullptr;

	ID3D11ComputeShader* MarchingCubesShader = nullptr;

	// Buffers
	ID3D11Buffer* inputBuffer = nullptr;
	ID3D11Buffer* outputBuffer = nullptr;
	ID3D11Buffer*  outputResultBuffer = nullptr;

	ID3D11Buffer* histogramBuffer;  
	ID3D11UnorderedAccessView* histogramUAV = nullptr;

	ID3D11Buffer* groupPrefBuffer = nullptr;
	ID3D11UnorderedAccessView* groupPrefUAV = nullptr;

	ID3D11Buffer* globalsBuffer = nullptr;
	ID3D11UnorderedAccessView* globalsUAV = nullptr;

	ID3D11Buffer* groupBaseBuffer = nullptr;
	ID3D11UnorderedAccessView* groupBaseUAV = nullptr;

	ID3D11Buffer* g_pParticlePositionBuffer = nullptr;
	ID3D11ShaderResourceView* g_pParticlePositionSRV = nullptr;
	ID3D11UnorderedAccessView* g_pParticlePositionUAV = nullptr;

	// Marching Cubes
	ID3D11Buffer* vertexBuffer = nullptr;
	ID3D11UnorderedAccessView* vertexUAV = nullptr;

	ID3D11Buffer* indexBuffer = nullptr;
	ID3D11UnorderedAccessView* indexUAV = nullptr;

	ID3D11Buffer* vertexCountBuffer = nullptr;
	ID3D11UnorderedAccessView* vertexCountUAV = nullptr;

	ID3D11Buffer* indexCountBuffer = nullptr;
	ID3D11UnorderedAccessView* indexCountUAV = nullptr;

	// Constant Buffers
	ID3D11Buffer*  SpatialGridConstantBuffer = nullptr;
	ID3D11Buffer*  BitonicSortConstantBuffer = nullptr;
	ID3D11Buffer*  RadixSortConstantBuffer = nullptr;

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

	// Marching Cubes
	std::unique_ptr<MarchingCubes> marchingCubes;
	std::vector<float> scalarField;

	std::vector<SimpleVertex> MarchingCubesVertices;
	std::vector<DWORD> MarchingCubesIndices;
	ID3D11Buffer* _pVertexBufferMarchingCubes;
	ID3D11Buffer* _pIndexBufferMarchingCubes;

	float cellSize = 1.0f;
	int gridSizeX = 64;
	int gridSizeY = 64;
	int gridSizeZ = 64;
	float smoothingRadius = 2.5f;
	float isoLevel = 75.0f;
	XMFLOAT3 gridOrigin = XMFLOAT3(0.0f, 0.0f, 0.0f);
};

