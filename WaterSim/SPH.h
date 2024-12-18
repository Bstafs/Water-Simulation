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
};

class SPH
{
public:
	SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime, float minX, float maxX);


private:
	
	void InitParticles();
	void InitComputeShader();

	void UpdateSpatialGrid();
	void UpdateComputeShader(float deltaTime);

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

	void InitRandomDirections();
	XMFLOAT3 GetRandomDir();

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
	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11ComputeShader* FluidSimComputeShader = nullptr;

	// Buffers
	ID3D11Buffer* inputBuffer = nullptr;
	ID3D11Buffer* outputBuffer = nullptr;
	ID3D11Buffer*  outputResultBuffer = nullptr;

	// SRV & UAV
	ID3D11ShaderResourceView* inputView = nullptr;
	ID3D11UnorderedAccessView* outputUAV = nullptr;

	ID3D11UnorderedAccessView* uavViewNull[1] = { nullptr };
	ID3D11ShaderResourceView* srvNull[2] = { nullptr, nullptr };
};

