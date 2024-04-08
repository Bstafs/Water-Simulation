#pragma once

#define GRID_DIMENSION 256

#include "Particle.h"
#include "Includes.h"

using namespace Microsoft::WRL;


struct ParticleConstantBuffer
{
	int particleCount;
	float wallStiffness;
	float deltaTime;
	float gravity;

	XMFLOAT4 vPlanes[6];
};


struct IntegrateParticle
{
	XMFLOAT3 position;
	float padding01;

	XMFLOAT3 velocity;
	float padding02;
};

class SPH
{
public:
	SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device);
	~SPH();
	void Update(float deltaTime);
	// Particle Variables

	int numberOfParticles;

	float sphGravity;

	XMFLOAT3 particlePositionValue;
	XMFLOAT3 particleVelocityValue;

	// Particle List
	std::vector <Particle*> particleList;

	XMFLOAT3* position;
	XMFLOAT3* velocity;

	XMFLOAT3 halfBoundSize = XMFLOAT3(10.0f / 2 - 1.0f, 10.0f / 2 - 1.0f, 10.0f / 2 - 1.0f);

	float collisionBoxSize = 10.0f;

	float dt;

	ID3DUserDefinedAnnotation* _pAnnotation = nullptr;
private:
	// Particle Initialization

	void InitParticles();

	void SetUpParticleConstantBuffer();
	void ParticleForcesSetup();

	static float SmoothingKernel(float radius, float dst);
	float CalculateDensity(XMFLOAT3 samplePoint);

	ID3D11DeviceContext* deviceContext;
	ID3D11Device* device;

	// Compute Shaders

	ParticleConstantBuffer particleConstantCPUBuffer;

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11Buffer* pParticleConstantBuffer = nullptr;

	// Integrate
	ID3D11ComputeShader* pParticleIntegrateCS = nullptr;

	ID3D11Buffer* pIntegrateBufferOne = nullptr;
	ID3D11Buffer* pIntegrateBufferTwo = nullptr;

	ID3D11ShaderResourceView* pIntegrateSRVOne = nullptr;
	ID3D11ShaderResourceView* pIntegrateSRVTwo = nullptr;

	ID3D11UnorderedAccessView* pIntegrateUAVOne = nullptr;
	ID3D11UnorderedAccessView* pIntegrateUAVTwo = nullptr;

	bool bufferIsSwapped = false;

	//Debug
	ID3D11Buffer* pDebugDensityBuffer = nullptr;
	ID3D11Buffer* pDebugForceBuffer = nullptr;
	ID3D11Buffer* pDebugPositionBuffer = nullptr;
	ID3D11Buffer* pDebugGridBuffer = nullptr;
	ID3D11Buffer* pDebugGridIndicesBuffer = nullptr;

	ID3D11UnorderedAccessView* uavViewNull[1] = { nullptr };
	ID3D11ShaderResourceView* srvNull[2] = { nullptr, nullptr };
};

