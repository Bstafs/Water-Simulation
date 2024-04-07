#include "SPH.h"

// Numthreads size for the sort
const UINT BITONIC_BLOCK_SIZE = 1024;
const UINT TRANSPOSE_BLOCK_SIZE = 64;
const UINT NUM_GRID_INDICES = 65536;
//walls
float particleSpacing = 5.0f;

constexpr float mapZ = 1.0f;
constexpr float mapX = 1.0f;
constexpr float mapY = 1.0f;

const float low_wall = 1.0f;
const float left_wall = 1.5f;
const float near_wall = 64.0f * particleSpacing;

SPH::SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device)
{
	// Variable Initialization
	numberOfParticles = numbParticles;

	deviceContext = contextdevice;
	this->device = device;

	// Particle Resize - Since I'm Going to be looping through the particles 3 times for the x,y,z position.
	int particleResize = numberOfParticles;
	particleList.resize(particleResize);

	position = new XMFLOAT3[numberOfParticles];
	velocity = new XMFLOAT3[numberOfParticles];

	// Particle Initialization
	InitParticles();
}

SPH::~SPH()
{
	// Particle Cleanup
	if (!particleList.empty())
	{
		particleList.clear();
		for (auto particlesL : particleList)
		{
			delete particlesL;
			particlesL = nullptr;
		}
	}

	if (pParticleIntegrateCS) pParticleIntegrateCS->Release();
	if (pIntegrateBufferOne) pIntegrateBufferOne->Release();
	if (pIntegrateUAVOne) pIntegrateUAVOne->Release();
	if (pIntegrateSRVOne) pIntegrateSRVOne->Release();
	if (pIntegrateBufferTwo) pIntegrateBufferTwo->Release();
	if (pIntegrateUAVTwo) pIntegrateUAVTwo->Release();
	if (pIntegrateSRVTwo) pIntegrateSRVTwo->Release();
}

void SPH::InitParticles()
{
	int particlesPerRow = (int)sqrt(numberOfParticles);
	int particlesPerCol = 5;
	float spacing = 1.0f * 2 + 1.0f;

	for (int i = 0; i < numberOfParticles; i++)
	{
		Particle* newParticle = new Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));

		float x = (i % particlesPerRow - particlesPerRow / 2.0f + 0.5f) * spacing;
		float y = (i / particlesPerRow % particlesPerCol - particlesPerCol / 2.0f + 0.5f) * spacing;
		float z = (i / (particlesPerRow * particlesPerCol) - 10.0f / 2.0f + 0.5f) * spacing;

		newParticle->position.x = x;
		newParticle->position.y = y;
		newParticle->position.z = z;
		
		particleList[i] = newParticle;
	}

	//--ConstantBuffer
	pParticleConstantBuffer = CreateConstantBuffer(sizeof(ParticleConstantBuffer), device, true);
	SetDebugName(pParticleConstantBuffer, "Particle Constant Buffer");

	//---Integrate
	IntegrateParticle* integrateData = new IntegrateParticle[numberOfParticles];
	for (int i = 0; i < numberOfParticles; ++i)
	{

		integrateData[i].position.x = particleList[i]->position.x;
		integrateData[i].position.y = particleList[i]->position.y;
		integrateData[i].position.z = particleList[i]->position.z;

		integrateData[i].velocity.x = particleList[i]->velocity.x;
		integrateData[i].velocity.y = particleList[i]->velocity.y;
		integrateData[i].velocity.z = particleList[i]->velocity.z;
	}

	pParticleIntegrateCS = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	SetDebugName(pParticleIntegrateCS, "Integrate Shader");

	pIntegrateBufferOne = CreateStructureBuffer(sizeof(IntegrateParticle), (float*)integrateData, numberOfParticles, device);
	SetDebugName(pIntegrateBufferOne, "Integrate Buffer One");

	pIntegrateBufferTwo = CreateStructureBuffer(sizeof(IntegrateParticle), (float*)integrateData, numberOfParticles, device);
	SetDebugName(pIntegrateBufferTwo, "Integrate Buffer Two");


	pIntegrateSRVOne = CreateShaderResourceView(pIntegrateBufferOne, numberOfParticles, device);
	SetDebugName(pIntegrateSRVOne, "Integrate SRV One");

	pIntegrateSRVTwo = CreateShaderResourceView(pIntegrateBufferTwo, numberOfParticles, device);
	SetDebugName(pIntegrateSRVTwo, "Integrate SRV Two");

	pIntegrateUAVOne = CreateUnorderedAccessView(pIntegrateBufferOne, numberOfParticles, device);
	SetDebugName(pIntegrateUAVOne, "Integrate UAV One");

	pIntegrateUAVTwo = CreateUnorderedAccessView(pIntegrateBufferTwo, numberOfParticles, device);
	SetDebugName(pIntegrateUAVTwo, "Integrate UAV Two");

	pDebugPositionBuffer = CreateReadableStructureBuffer(sizeof(IntegrateParticle) * numberOfParticles, nullptr, device);
	SetDebugName(pDebugPositionBuffer, "Integrate Readable Structure Buffer");
}

void SPH::SetUpParticleConstantBuffer()
{
	particleConstantCPUBuffer.particleCount = numberOfParticles;
	particleConstantCPUBuffer.wallStiffness = 3.0f;
	particleConstantCPUBuffer.deltaTime = dt;
	particleConstantCPUBuffer.gravity = 9.81f;

	particleConstantCPUBuffer.vPlanes[0] = XMFLOAT4(0.0f, 0.0f, 1.0f, low_wall); // Back
	particleConstantCPUBuffer.vPlanes[1] = XMFLOAT4(0.0f, 0.0f, -1.0f, low_wall); // Front

	particleConstantCPUBuffer.vPlanes[2] = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f); // Bottom Starts at 0.0f
	particleConstantCPUBuffer.vPlanes[3] = XMFLOAT4(0.0f, -1.0f, 0.0f, 32.0f); // Top

	particleConstantCPUBuffer.vPlanes[4] = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f); // Left
	particleConstantCPUBuffer.vPlanes[5] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 32.0f); // Right

	UpdateBuffer((float*)&particleConstantCPUBuffer, sizeof(ParticleConstantBuffer), pParticleConstantBuffer, deviceContext);
}

void SPH::ParticleForcesSetup()
{
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Forces");

	SetUpParticleConstantBuffer();

	// Integrate
	{
		_pAnnotation->BeginEvent(L"Integrate Stage");
		deviceContext->Flush();

		deviceContext->CSSetShader(pParticleIntegrateCS, nullptr, 0);

		if (bufferIsSwapped == false)
		{
			deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVOne);
			deviceContext->CSSetUnorderedAccessViews(0, 1, &pIntegrateUAVTwo, nullptr);
		}
		else
		{
			deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVTwo);
			deviceContext->CSSetUnorderedAccessViews(0, 1, &pIntegrateUAVOne, nullptr);
		}

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(numberOfParticles, 1, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
		deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetShaderResources(1, 1, srvNull);

		deviceContext->CopyResource(pDebugPositionBuffer, pIntegrateBufferOne);
		IntegrateParticle* positions = (IntegrateParticle*)MapBuffer(pDebugPositionBuffer, deviceContext);
		// Integrate

		for (int i = 0; i < numberOfParticles; ++i)
		{
			particleList[i]->position.x = positions->position.x;
			particleList[i]->position.y = positions->position.y;
			particleList[i]->position.z = positions->position.z;
		}
		UnMapBuffer(pDebugPositionBuffer, deviceContext);

		_pAnnotation->EndEvent();

		bufferIsSwapped = !bufferIsSwapped;

	}

	_pAnnotation->EndEvent();
}


void SPH::Update(float deltaTime)
{
	// Setup Particle Forces
   	//ParticleForcesSetup();

	float dampingFactor = 0.1f;

	float minX = -10.0f;
	float maxX = 10.0f;
	float minY = -10.0f;
	float maxY = 20.0f;
	float minZ = -10.0f;
	float maxZ = 10.0f;

	for (int i = 0; i < particleList.size(); i++)
	{
		particleList[i]->velocity.x += deltaTime;
		particleList[i]->velocity.y += -9.81f * deltaTime;
		particleList[i]->velocity.x += deltaTime;

		particleList[i]->position.x += particleList[i]->velocity.x * deltaTime;
		particleList[i]->position.y += particleList[i]->velocity.y * deltaTime;
		particleList[i]->position.z += particleList[i]->velocity.z * deltaTime;

		// Basic Bounding Box
		{ 
			if (particleList[i]->position.x < minX) {
				particleList[i]->position.x = minX;
				particleList[i]->velocity.x = -particleList[i]->velocity.x * dampingFactor; // Reverse velocity upon collision
			}
			else if (particleList[i]->position.x > maxX) {
				particleList[i]->position.x = maxX;
				particleList[i]->velocity.x = -particleList[i]->velocity.x * dampingFactor;
			}

			if (particleList[i]->position.y < minY) {
				particleList[i]->position.y = minY;
				particleList[i]->velocity.y = -particleList[i]->velocity.y * dampingFactor;
			}
			else if (particleList[i]->position.y > maxY) {
				particleList[i]->position.y = maxY;
				particleList[i]->velocity.y = -particleList[i]->velocity.y * dampingFactor;
			}

			if (particleList[i]->position.z < minZ) {
				particleList[i]->position.z = minZ;
				particleList[i]->velocity.z = -particleList[i]->velocity.z * dampingFactor;
			}
			else if (particleList[i]->position.z > maxZ) {
				particleList[i]->position.z = maxZ;
				particleList[i]->velocity.z = -particleList[i]->velocity.z * dampingFactor;
			}
		} // Basic Bounding Box
	}
}