#include "SPH.h"


//walls
float particleSpacing = 1.0f;

const float low_wall = 1.0f;
const float left_wall = 1.5f;
const float near_wall = 32.0f * particleSpacing;


SPH::SPH(int numbParticles, float mass, float density, float gasConstant, float viscosity, float h, float g, float tension, float elasticity, float pressure, ID3D11DeviceContext* contextdevice, ID3D11Device* device)
{
	// Variable Initialization
	numberOfParticles = numbParticles;
	sphDensity = density;
	sphViscosity = viscosity;
	sphH = h;
	sphG = g;
	sphTension = tension;
	sphElasticity = elasticity;
	sphPressure = pressure;

	deviceContext = contextdevice;
	this->device = device;

	MASS_CONSTANT = mass;

	// Kernel Smoothing Constants Initialization
	POLY6_CONSTANT = MASS_CONSTANT * 315.0f / (64.0f * PI * powf(sphH, 9));

	SPIKY_CONSTANT = MASS_CONSTANT * 45.0f / (PI * powf(sphH, 6));
	SPIKYGRAD_CONSTANT = MASS_CONSTANT * -45.0f / (PI * powf(sphH, 6));

	VISC_CONSTANT = MASS_CONSTANT * sphViscosity * 45.0f / (PI * powf(sphH, 6));

	// Particle Constant Initialization

	GAS_CONSTANT = gasConstant;
	H2_CONSTANT = sphH * sphH;
	DENS_CONSTANT = MASS_CONSTANT * POLY6_CONSTANT * pow(sphH, 6);

	// Particle Resize - Since I'm Going to be looping through the particles 3 times for the x,y,z position.
	int particleResize = numberOfParticles;
	particleList.resize(particleResize);

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
	if (pIntegrateBuffer) pIntegrateBuffer->Release();
	if (pIntegrateUAV) pIntegrateUAV->Release();
	if (pIntegrateSRV) pIntegrateSRV->Release();

	if (pParticleDensityCS) pParticleDensityCS->Release();
	if (pDensitySRV) pDensitySRV->Release();
	if (pDensityUAV) pDensityUAV->Release();
	if (pDensityBuffer) pDensityBuffer->Release();

	if (pParticleForcesCS) pParticleForcesCS->Release();
	if (pForcesSRV) pForcesSRV->Release();
	if (pForcesUAV) pForcesUAV->Release();
	if (pForcesBuffer) pForcesBuffer->Release();
}

void SPH::InitParticles()
{
	// Following Realtime Particle I researched, I need to loop over every particle for x,y,z

	const UINT startingHeight = 7;

	for (int i = 0; i < numberOfParticles; ++i)
	{
		UINT x = i % (startingHeight);
		UINT y = i / startingHeight % (startingHeight);
		UINT z = i % (startingHeight);

		XMFLOAT3 startingParticlePosition = XMFLOAT3((float)x * particleSpacing, -1.6f, (float)y * particleSpacing);

		Particle* newParticle = new Particle(MASS_CONSTANT, 0.6f, startingParticlePosition, XMFLOAT3(1, 1, 1));

		newParticle->elasticity = sphElasticity;
		newParticle->acceleration = XMFLOAT3(1, 1, 1);
		newParticle->force = XMFLOAT3(1, 1, 1);


		particleList[i] = newParticle;
	}

	//--ConstantBuffer
	pParticleConstantBuffer = CreateConstantBuffer(sizeof(ParticleConstantBuffer), device, true);
	pSortConstantBuffer = CreateConstantBuffer(sizeof(SortConstantBuffer), device, true);

	//---Integrate
	IntegrateParticle* integrateData = new IntegrateParticle[numberOfParticles];
	for (int i = 0; i < numberOfParticles; ++i)
	{
		Particle* particle = particleList[i];

		integrateData->position.x = particle->position.x;
		integrateData->position.y = particle->position.y;
		integrateData->position.z = particle->position.z;

		integrateData->velocity.x = particle->velocity.x;
		integrateData->velocity.y = particle->velocity.y;
		integrateData->velocity.z = particle->velocity.z;

		integrateData->padding01 = 0.0f;
		integrateData->padding02 = 0.0f;
	}

	pParticleIntegrateCS = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	pIntegrateBuffer = CreateStructureBuffer(sizeof(IntegrateParticle), (float*)integrateData, numberOfParticles, device);
	pIntegrateSRV = CreateShaderResourceView(pIntegrateBuffer, numberOfParticles, device);
	pIntegrateUAV = CreateUnorderedAccessView(pIntegrateBuffer, numberOfParticles, device);
	pDebugPositionBuffer = CreateReadableStructureBuffer(sizeof(IntegrateParticle) * numberOfParticles, nullptr, device);

	//---Acceleration 
	ParticleForces* forceData = new ParticleForces[numberOfParticles];
	for (int i = 0; i < numberOfParticles; ++i)
	{
		Particle* particle = particleList[i];

		forceData->acceleration.x = particle->acceleration.x;
		forceData->acceleration.y = particle->acceleration.y;
		forceData->acceleration.z = particle->acceleration.z;

		forceData->padding01 = 0.0f;
	}
	pParticleForcesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "CSForcesMain", device);
	pForcesBuffer = CreateStructureBuffer(sizeof(ParticleForces), (float*)forceData, numberOfParticles, device);
	pForcesUAV = CreateUnorderedAccessView(pForcesBuffer, numberOfParticles, device);
	pForcesSRV = CreateShaderResourceView(pForcesBuffer, numberOfParticles, device);
	pDebugForceBuffer = CreateReadableStructureBuffer(sizeof(ParticleForces) * numberOfParticles, nullptr, device);

	//--Density
	ParticleDensity* densityData = new ParticleDensity[numberOfParticles];
	for (int i = 0; i < numberOfParticles; ++i)
	{
		Particle* particle = particleList[i];

		densityData->density = particle->density;
		densityData->padding01 = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	pParticleDensityCS = CreateComputeShader(L"SPHComputeShader.hlsl", "CSDensityMain", device);
	pDensityBuffer = CreateStructureBuffer(sizeof(ParticleDensity), (float*)densityData, numberOfParticles, device);
	pDensityUAV = CreateUnorderedAccessView(pDensityBuffer, numberOfParticles, device);
	pDensitySRV = CreateShaderResourceView(pDensityBuffer, numberOfParticles, device);
	pDebugDensityBuffer = CreateReadableStructureBuffer(sizeof(ParticleDensity) * numberOfParticles, nullptr, device);

	//--Build Grid
	pParticleGridCS = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridCS", device);
	pGridBuffer = CreateStructureBuffer(sizeof(GridKeyStructure), nullptr, numberOfParticles, device);
	pGridUAV = CreateUnorderedAccessView(pGridBuffer, numberOfParticles, device);
	pGridSRV = CreateShaderResourceView(pGridBuffer, numberOfParticles, device);
	pDebugGridBuffer = CreateReadableStructureBuffer(sizeof(GridKeyStructure) * numberOfParticles, nullptr, device);

	//--Build Grid Sorter
	pGridSorterShader = CreateComputeShader(L"SPHComputeShader.hlsl", "SortGridIndices", device);

	//--Build Grid Indices
	pParticleGridIndicesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridIndicesCS", device);
	pGridIndicesBuffer = CreateStructureBuffer(sizeof(GridBorderStructure), nullptr, 256 * 256 * 256, device);
	pGridIndicesUAV = CreateUnorderedAccessView(pGridBuffer, 256 * 256 * 256, device);
	pGridIndicesSRV = CreateShaderResourceView(pGridBuffer, 256 * 256 * 256, device);
	pDebugGridIndicesBuffer = CreateReadableStructureBuffer(sizeof(GridKeyStructure) * 256 * 256 * 256, nullptr, device);

	//--Clear Grid Indices
	pParticleGridIndicesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "ClearGridIndicesCS", device);
}

void SPH::ParticleBoxCollision()
{
	for (int i = 0; i < particleList.size(); ++i)
	{
		Particle* part = particleList[i];

		// Creating a box Collsion to hold particles. Continuining Following Realtime Particle - Based Fluid Simulation.

		// Collision on the y Axis

		// Top
		if (part->position.y < part->size - collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * (part->size - collisionBoxSize);
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Bottom
		if (part->position.y > -part->size + collisionBoxSize)
		{
			part->position.y = -part->position.y + 2 * -(part->size - collisionBoxSize);
			part->velocity.y = -part->velocity.y * part->elasticity;
		}

		// Collision on the X Axis

		// Left
		if (part->position.x < part->size - collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * (part->size - collisionBoxSize);
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Right
		if (part->position.x > -part->size + collisionBoxSize)
		{
			part->position.x = -part->position.x + 2 * -(part->size - collisionBoxSize);
			part->velocity.x = -part->velocity.x * part->elasticity;
		}

		// Collision on the Z Axis

		// Back
		if (part->position.z < part->size - collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * (part->size - collisionBoxSize);
			part->velocity.z = -part->velocity.z * part->elasticity;
		}

		// Front
		if (part->position.z > -part->size + collisionBoxSize)
		{
			part->position.z = -part->position.z + 2 * -(part->size - collisionBoxSize);
			part->velocity.z = -part->velocity.z * part->elasticity;
		}
	}
}

void SPH::Update()
{
	ParticleBoxCollision();
}

void SPH::BuildGrid()
{
	deviceContext->Flush();

	deviceContext->CSSetShader(pParticleGridCS, nullptr, 0);

	deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRV);

	deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);

	deviceContext->Dispatch(256, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(0, 1, srvNull);
	deviceContext->CSSetShaderResources(3, 1, srvNull);
}

void SPH::SortGridIndices()
{
	// Bitonic Sort

	deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);


	const UINT NUM_ELEMENTS = numberOfParticles;
	const UINT MATRIX_WIDTH = 1024;
	const UINT MATRIX_HEIGHT = numberOfParticles / MATRIX_WIDTH;
	const UINT WARP_GROUP_SIZE = 1024;

	// Sort
	for (UINT level = 2; level <= WARP_GROUP_SIZE; level <<= 1)
	{
		sortConstantCPUBuffer.sortLevel = level;
		sortConstantCPUBuffer.sortAlternateMask = level;

		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);
		deviceContext->CSSetConstantBuffers(2, 1, &pSortConstantBuffer);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);

		deviceContext->Dispatch(numberOfParticles / WARP_GROUP_SIZE, 1, 1);
	}
	// Transpose
	for (UINT level = (WARP_GROUP_SIZE << 1); level <= numberOfParticles; level <<= 1)
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;

		sortConstantCPUBuffer.sortLevel = level / WARP_GROUP_SIZE;
		sortConstantCPUBuffer.sortAlternateMask = (level & ~NUM_ELEMENTS) / WARP_GROUP_SIZE;
		sortConstantCPUBuffer.iWidth = MATRIX_WIDTH;
		sortConstantCPUBuffer.iHeight = MATRIX_HEIGHT;
		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);
		deviceContext->CSSetConstantBuffers(2, 1, &pSortConstantBuffer);

		//deviceContext->CSSetUnorderedAccessViews()
	}

	// Sort the transpose

	// transpose to the original

	// sort the original
}

void SPH::BuildGridIndices()
{
	// Clear

	deviceContext->Flush();
	deviceContext->CSSetShader(pParticleClearGridIndicesCS, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAV, nullptr);
	deviceContext->Dispatch(256 * 256 * 256 / 1024, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShaderResources(4, 1, srvNull);
	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);


	// Build
	deviceContext->Flush();
	deviceContext->CSSetShader(pParticleGridIndicesCS, nullptr, 0);

	deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
	deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRV);

	deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAV, nullptr);

	deviceContext->Dispatch(256, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);

	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(4, 1, srvNull);

	deviceContext->Flush();
}


void SPH::SetUpParticleConstantBuffer()
{
	particleConstantCPUBuffer.particleCount = numberOfParticles;
	particleConstantCPUBuffer.wallStiffness = 3000.0f;
	particleConstantCPUBuffer.padding00 = XMFLOAT2(0.0f, 0.0f);
	particleConstantCPUBuffer.deltaTime = 1.0f / 60.0f;
	particleConstantCPUBuffer.smoothingLength = sphH;
	particleConstantCPUBuffer.pressure = 200.0f;
	particleConstantCPUBuffer.restDensity = sphDensity;
	particleConstantCPUBuffer.densityCoef = MASS_CONSTANT * 315.0f / (64.0f * PI * powf(sphH, 9));;
	particleConstantCPUBuffer.GradPressureCoef = MASS_CONSTANT * -45.0f / (XM_PI * powf(sphH, 6));;
	particleConstantCPUBuffer.LapViscosityCoef = MASS_CONSTANT * sphViscosity * 45.0f / (XM_PI * powf(sphH, 6));;
	particleConstantCPUBuffer.gravity = sphG;

	particleConstantCPUBuffer.vPlanes[0] = XMFLOAT4(0, 0, 1, low_wall);
	particleConstantCPUBuffer.vPlanes[1] = XMFLOAT4(0, 0, -1, low_wall);
	particleConstantCPUBuffer.vPlanes[2] = XMFLOAT4(0, 1, 0, 0);
	particleConstantCPUBuffer.vPlanes[3] = XMFLOAT4(0, -1, 0, near_wall);
	particleConstantCPUBuffer.vPlanes[4] = XMFLOAT4(1, 0, 1, 0);
	particleConstantCPUBuffer.vPlanes[5] = XMFLOAT4(-1, 0, 1, near_wall);

	UpdateBuffer((float*)&particleConstantCPUBuffer, sizeof(ParticleConstantBuffer), pParticleConstantBuffer, deviceContext);
}

void SPH::ParticleForcesSetup()
{
	SetUpParticleConstantBuffer();

	// Density Calculations
	{
		deviceContext->Flush();

		deviceContext->CSSetShader(pParticleDensityCS, nullptr, 0);

		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRV);
		deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
		deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRV);

		ID3D11UnorderedAccessView* cleanerUAV = nullptr;
		deviceContext->CSSetUnorderedAccessViews(2, 1, &cleanerUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(2, 1, &pDensityUAV, nullptr);

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(256, 1, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);

		deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

		deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetShaderResources(3, 1, srvNull);
		deviceContext->CSSetShaderResources(4, 1, srvNull);


		deviceContext->CopyResource(pDebugDensityBuffer, pDensityBuffer);
		ParticleDensity* densities = reinterpret_cast<ParticleDensity*>(MapBuffer(pDebugDensityBuffer, deviceContext));
		for (int i = 0; i < numberOfParticles; ++i)
		{
			Particle* particle = particleList[i];

			particle->density = densities->density;
		}
		UnMapBuffer(pDebugDensityBuffer, deviceContext);

		deviceContext->Flush();
	}

	//Force Calculations
	{
		deviceContext->Flush();

		deviceContext->CSSetShader(pParticleForcesCS, nullptr, 0);

		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRV);
		deviceContext->CSSetShaderResources(2, 1, &pDensitySRV);
		deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
		deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRV);

		ID3D11UnorderedAccessView* cleanerUAV = nullptr;
		deviceContext->CSSetUnorderedAccessViews(1, 1, &cleanerUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(1, 1, &pForcesUAV, nullptr);

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(256, 1, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
		deviceContext->CSSetShaderResources(1, 1, srvNull);


		deviceContext->CopyResource(pDebugForceBuffer, pForcesBuffer);

		ParticleForces* forces = reinterpret_cast<ParticleForces*>(MapBuffer(pDebugForceBuffer, deviceContext));
		for (int i = 0; i < numberOfParticles; ++i)
		{
			Particle* particle = particleList[i];

			particle->acceleration = forces->acceleration;
		}
		UnMapBuffer(pDebugForceBuffer, deviceContext);

		deviceContext->Flush();
	}


	// Integrate
	{
		deviceContext->Flush();

		deviceContext->CSSetShader(pParticleIntegrateCS, nullptr, 0);
		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRV);
		deviceContext->CSSetShaderResources(1, 1, &pForcesSRV);
		deviceContext->CSSetUnorderedAccessViews(0, 1, &pIntegrateUAV, nullptr);

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(256, 1, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
		deviceContext->CSSetShaderResources(0, 1, srvNull);

		deviceContext->CopyResource(pDebugPositionBuffer, pIntegrateBuffer);
		IntegrateParticle* positions = (IntegrateParticle*)MapBuffer(pDebugPositionBuffer, deviceContext);
		for (int i = 0; i < numberOfParticles; ++i)
			// Integrate
		{
			Particle* particle = particleList[i];

			particle->velocity.x += positions->velocity.x;
			particle->velocity.y += positions->velocity.y;
			particle->velocity.z += positions->velocity.z;

			particle->position.x += positions->position.x;
			particle->position.y += positions->position.y;
			particle->position.z += positions->position.z;
		}
		UnMapBuffer(pDebugPositionBuffer, deviceContext);

	}
}

void SPH::RenderFluid()
{

}


void SPH::Draw()
{
	// Build Grid
	BuildGrid();

	// Sort Grid Indices
	SortGridIndices();

	// Build Grid Indices
	BuildGridIndices();

	// Setup Particle Forces
	ParticleForcesSetup();

	// Render Fluid
	RenderFluid();
}