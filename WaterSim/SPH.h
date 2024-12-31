#pragma once

#define PI 3.141592653589793238462643383279502884197


#include "Particle.h"
#include "SpatialGrid.h"

struct ParticlePosition
{
	XMFLOAT3 position;
	float deltaTime;

	XMFLOAT3 velocity;
	float density;

	float padding; // 12 bytes
	float nearDensity; // 4 bytes (16-byte aligned)
	float minX;
	float maxX;
};

struct SimulationParams 
{
	float cellSize; // Size of each grid cell
	int gridResolution; // Number of cells along one axis
	int maxParticlesPerCell; // Maximum particles a cell can hold
	int numParticles; // Total number of particles
};

class SPH
{
public:
	SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime, float minX, float maxX);


private:
	
	// Initial Particle Positions
	void InitParticles();

	// GPU Side
	void InitSpatialGridClear();
	void InitAddParticlesToSpatialGrid();
	void InitParticleDensities();
	void InitParticlePressure();
	void InitComputeIntegrateShader();

	void UpdateSpatialGrid();
	void UpdateSpatialGridClear(float deltaTime);
	void UpdateAddParticlesToSpatialGrid(float deltaTime);
	void UpdateParticleDensities(float deltaTime);
	void UpdateParticlePressure(float deltaTime);
	void UpdateIntegrateComputeShader(float deltaTime, float minX, float maxX);

	void SwapBuffersSpatialGrid();
	void SwapBuffersIntegrate();
	bool isBufferSwapped = false;
	// CPU Side
	static float DensitySmoothingKernel(float dst, float radius);
	static float PressureSmoothingKernel(float radius, float dst); // Derivative of Density Kernel
	static float NearDensitySmoothingKernel(float radius, float dst);
	static float NearDensitySmoothingKernelDerivative(float radius, float dst);
	static float ViscositySmoothingKernel(float radius, float dst);

	float CalculateMagnitude(const XMFLOAT3& vector);
	float CalculateDensity(const XMFLOAT3& samplePoint);
	float CalculateNearDensity(const XMFLOAT3& samplePoint);
	float ConvertDensityToPressure(float density);
	float CalculateSharedPressure(float densityA, float densityB);
	XMFLOAT3 CalculatePressureForceWithRepulsion(int particleIndex);

public:
	std::vector <Particle*> particleList;
	ID3DUserDefinedAnnotation* _pAnnotation = nullptr;
private:
	// Particle Initialization
	float dampingFactor = 0.99f;

//	float minX = -10.0f, maxX = 10.0f;
	float minY = -10.0f, maxY = 20.0f;
	float minZ = -10.0f, maxZ = 10.0f;

	float targetDensity = 8.0f;
	float stiffnessValue = 30.0f;

	std::vector<XMFLOAT3> randomDirections;
	int numRandomDirections = 1000;

	SpatialGrid spatialGrid;

	ParticlePosition* position;
	float mGravity = 0.0f;
	vector<XMFLOAT3> predictedPositions;

	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11ComputeShader* FluidSimIntegrateShader = nullptr;
	ID3D11ComputeShader* SpatialGridClearShader = nullptr;
	ID3D11ComputeShader* SpatialGridAddParticleShader = nullptr;
	ID3D11ComputeShader* FluidSimCalculateDensity = nullptr;
	ID3D11ComputeShader* FluidSimCalculatePressure = nullptr;


	// Buffers
	ID3D11Buffer* inputBuffer = nullptr;
	ID3D11Buffer* outputBuffer = nullptr;
	ID3D11Buffer*  outputResultBuffer = nullptr;

	ID3D11Buffer*  SpatialGridConstantBuffer = nullptr;

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

