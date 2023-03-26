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
	//if (pIntegrateBuffer) pIntegrateBuffer->Release();
	//if (pIntegrateUAV) pIntegrateUAV->Release();
	//if (pIntegrateSRV) pIntegrateSRV->Release();

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
		newParticle->velocity = XMFLOAT3(1, 1, 1);
		newParticle->force = XMFLOAT3(1, 1, 1);


		particleList[i] = newParticle;
	}

	//--ConstantBuffer
	pParticleConstantBuffer = CreateConstantBuffer(sizeof(ParticleConstantBuffer), device, true);
	SetDebugName(pParticleConstantBuffer, "Particle Constant Buffer");

	pSortConstantBuffer = CreateConstantBuffer(sizeof(SortConstantBuffer), device, true);
	SetDebugName(pSortConstantBuffer, "Sort Constant Buffer");

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
	SetDebugName(pParticleForcesCS, "Particle Forces Shader");

	pForcesBuffer = CreateStructureBuffer(sizeof(ParticleForces), (float*)forceData, numberOfParticles, device);
	SetDebugName(pForcesBuffer, "Particle Forces Buffer");

	pForcesUAV = CreateUnorderedAccessView(pForcesBuffer, numberOfParticles, device);
	SetDebugName(pForcesUAV, "Particle Forces UAV");

	pForcesSRV = CreateShaderResourceView(pForcesBuffer, numberOfParticles, device);
	SetDebugName(pForcesSRV, "Particle Forces SRV");

	pDebugForceBuffer = CreateReadableStructureBuffer(sizeof(ParticleForces) * numberOfParticles, nullptr, device);
	SetDebugName(pDebugForceBuffer, "Particle Forces Readable Structure Buffer");

	//--Density
	ParticleDensity* densityData = new ParticleDensity[numberOfParticles];
	for (int i = 0; i < numberOfParticles; ++i)
	{
		Particle* particle = particleList[i];

		densityData->density = particle->density;
		densityData->padding01 = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	pParticleDensityCS = CreateComputeShader(L"SPHComputeShader.hlsl", "CSDensityMain", device);
	SetDebugName(pParticleDensityCS, "Particle Density Shader");

	pDensityBuffer = CreateStructureBuffer(sizeof(ParticleDensity), (float*)densityData, numberOfParticles, device);
	SetDebugName(pDensityBuffer, "Particle Density Buffer");

	pDensityUAV = CreateUnorderedAccessView(pDensityBuffer, numberOfParticles, device);
	SetDebugName(pDensityUAV, "Particle Density UAV");

	pDensitySRV = CreateShaderResourceView(pDensityBuffer, numberOfParticles, device);
	SetDebugName(pDensitySRV, "Particle Density SRV");

	pDebugDensityBuffer = CreateReadableStructureBuffer(sizeof(ParticleDensity) * numberOfParticles, nullptr, device);
	SetDebugName(pDebugDensityBuffer, "Particle Density Readable Structure Buffer");

	//--Build Grid
	pParticleGridCS = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridCS", device);
	SetDebugName(pParticleGridCS, "Particle Grid Shader");

	pGridBuffer = CreateStructureBuffer(sizeof(GridKeyStructure), nullptr, numberOfParticles, device);
	SetDebugName(pGridBuffer, "Particle Grid Buffer");

	pGridUAV = CreateUnorderedAccessView(pGridBuffer, numberOfParticles, device);
	SetDebugName(pGridUAV, "Particle Grid UAV");

	pGridSRV = CreateShaderResourceView(pGridBuffer, numberOfParticles, device);
	SetDebugName(pGridSRV, "Particle Grid SRV");

	pDebugGridBuffer = CreateReadableStructureBuffer(sizeof(GridKeyStructure) * numberOfParticles, nullptr, device);
	SetDebugName(pDebugGridBuffer, "Particle Grid Readable Structure Buffer");

	//--Build Grid Sorter
	pGridSorterShader = CreateComputeShader(L"SPHComputeShader.hlsl", "SortGridIndices", device);

	//--Build Grid Indices
	pParticleGridIndicesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridIndicesCS", device);
	SetDebugName(pParticleGridIndicesCS, "Particle Grid Indices Shader");

	pGridIndicesBuffer = CreateStructureBuffer(sizeof(GridBorderStructure), nullptr, 256 * 256 * 256, device);
	SetDebugName(pGridIndicesBuffer, "Particle Grid Indices Buffer");

	pGridIndicesUAV = CreateUnorderedAccessView(pGridIndicesBuffer, 256 * 256 * 256, device);
	SetDebugName(pGridIndicesUAV, "Particle Grid Indices UAV");

	pGridIndicesSRV = CreateShaderResourceView(pGridIndicesBuffer, 256 * 256 * 256, device);
	SetDebugName(pGridIndicesSRV, "Particle Grid Indices SRV");

	pDebugGridIndicesBuffer = CreateReadableStructureBuffer(sizeof(GridKeyStructure) * 256 * 256 * 256, nullptr, device);
	SetDebugName(pDebugGridIndicesBuffer, "Particle Grid Indices Readable Structure Buffer");

	// Transpose Sort Matrix
	pTransposeMatrixCS = CreateComputeShader(L"SPHComputeShader.hlsl", "TransposeMatrixCS", device);
	pGridIndicesTempBuffer = CreateStructureBuffer(sizeof(GridKeyStructure), nullptr, numberOfParticles, device);
	pGridIndicesTempUAV = CreateUnorderedAccessView(pGridIndicesTempBuffer, numberOfParticles, device);
	pGridIndicesTempSRV = CreateShaderResourceView(pGridIndicesTempBuffer, numberOfParticles, device);

	//--Clear Grid Indices
	pParticleClearGridIndicesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "ClearGridIndicesCS", device);
	SetDebugName(pParticleClearGridIndicesCS, "Particle Grid Clear Indices Shader");
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
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Build Grid");


	deviceContext->CSSetShader(pParticleGridCS, nullptr, 0);

	if (bufferIsSwapped == false)
	{
		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVOne);
	}
	else
	{
		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVTwo);
	}

	deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);

	deviceContext->Dispatch(256, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(0, 1, srvNull);
	deviceContext->CSSetShaderResources(3, 1, srvNull);

	_pAnnotation->EndEvent();
}

void SPH::SortGridIndices()
{
	// Bitonic Sort

	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Bitonic Sort");

	deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);

	const UINT NUM_ELEMENTS = numberOfParticles;
	const UINT MATRIX_WIDTH = 1024;
	const UINT MATRIX_HEIGHT = numberOfParticles / MATRIX_WIDTH;
	const UINT WARP_GROUP_SIZE = 1024;

	_pAnnotation->BeginEvent(L"Particle Sort");
	// Sort
	for (UINT level = 2; level <= WARP_GROUP_SIZE; level <<= 1)
	{
		sortConstantCPUBuffer.sortLevel = level;
		sortConstantCPUBuffer.sortAlternateMask = level;

		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);
		deviceContext->CSSetConstantBuffers(2, 1, &pSortConstantBuffer);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);

		deviceContext->Dispatch(numberOfParticles / WARP_GROUP_SIZE, 1, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);

		deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	}
	_pAnnotation->EndEvent();

	_pAnnotation->BeginEvent(L"Particle Transpose");
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

		deviceContext->CSSetShader(pTransposeMatrixCS, nullptr, 0);
		deviceContext->CSSetShaderResources(5, 1, &pGridIndicesTempSRV);
		deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &pGridIndicesTempUAV, nullptr);

		deviceContext->Dispatch(MATRIX_WIDTH, MATRIX_HEIGHT, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);

		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetShaderResources(5, 1, srvNull);
		deviceContext->CSSetShaderResources(3, 1, srvNull);

		deviceContext->Flush();

		deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);
		deviceContext->Dispatch(numberOfParticles / 1024, 1, 1);

		deviceContext->Flush();

		sortConstantCPUBuffer.sortLevel = WARP_GROUP_SIZE;
		sortConstantCPUBuffer.sortAlternateMask = level;
		sortConstantCPUBuffer.iWidth = MATRIX_HEIGHT;
		sortConstantCPUBuffer.iHeight = MATRIX_WIDTH;

		deviceContext->CSSetShader(pTransposeMatrixCS, nullptr, 0);
		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);
		deviceContext->CSSetConstantBuffers(2, 1, &pSortConstantBuffer);

		deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAV, nullptr);
		deviceContext->CSSetShaderResources(5, 1, &pGridIndicesTempSRV);

		deviceContext->Dispatch(MATRIX_HEIGHT, MATRIX_WIDTH, 1);

		deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
		deviceContext->CSSetShaderResources(5, 1, srvNull);

		deviceContext->Flush();

		deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);
		deviceContext->Dispatch(numberOfParticles / 1024, 1, 1);
		deviceContext->Flush();

	}
	_pAnnotation->EndEvent();

	// Sort the transpose

	// transpose to the original

	// sort the original

	_pAnnotation->EndEvent();
}

void SPH::BuildGridIndices()
{
	// Clear
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Clear Indices");


	deviceContext->Flush();
	deviceContext->CSSetShader(pParticleClearGridIndicesCS, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAV, nullptr);
	deviceContext->Dispatch(256 * 256 * 256 / 1024, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShaderResources(4, 1, srvNull);
	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);

	_pAnnotation->EndEvent();

	// Build
	deviceContext->Flush();

	_pAnnotation->BeginEvent(L"Particle Build Indices");

	deviceContext->CSSetShader(pParticleGridIndicesCS, nullptr, 0);

	deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
	deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRV);

	deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAV, nullptr);

	deviceContext->Dispatch(256 / 1024, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);

	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(4, 1, srvNull);

	_pAnnotation->EndEvent();
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
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Forces");

	SetUpParticleConstantBuffer();

	// Density Calculations
	{
		_pAnnotation->BeginEvent(L"Density Stage");
		deviceContext->Flush();

		deviceContext->CSSetShader(pParticleDensityCS, nullptr, 0);

		if (bufferIsSwapped == false)
		{
			deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVOne);
		}
		else
		{
			deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVTwo);
		}
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

		_pAnnotation->EndEvent();
	}

	// Force Calculations
	{
		_pAnnotation->BeginEvent(L"Acceleration Stage");
		deviceContext->Flush();

		deviceContext->CSSetShader(pParticleForcesCS, nullptr, 0);

		if (bufferIsSwapped == false)
		{
			deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVOne);
		}
		else
		{
			deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVTwo);
		}

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
		deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetShaderResources(2, 1, srvNull);
		deviceContext->CSSetShaderResources(3, 1, srvNull);
		deviceContext->CSSetShaderResources(4, 1, srvNull);


		deviceContext->CopyResource(pDebugForceBuffer, pForcesBuffer);

		ParticleForces* forces = reinterpret_cast<ParticleForces*>(MapBuffer(pDebugForceBuffer, deviceContext));
		for (int i = 0; i < numberOfParticles; ++i)
		{
			Particle* particle = particleList[i];

			particle->acceleration = forces->acceleration;
		}
		UnMapBuffer(pDebugForceBuffer, deviceContext);
		_pAnnotation->EndEvent();
	}

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

		deviceContext->CSSetShaderResources(1, 1, &pForcesSRV);

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(256, 1, 1);

		deviceContext->CSSetShader(nullptr, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
		deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetShaderResources(1, 1, srvNull);

		deviceContext->CopyResource(pDebugPositionBuffer, pIntegrateBufferOne);
		IntegrateParticle* positions = (IntegrateParticle*)MapBuffer(pDebugPositionBuffer, deviceContext);
		for (int i = 0; i < numberOfParticles; ++i)
			// Integrate
		{
			Particle* particle = particleList[i];

			particle->velocity.x = positions->velocity.x;
			particle->velocity.y = positions->velocity.y;
			particle->velocity.z = positions->velocity.z;

			particle->position.x = positions->position.x;
			particle->position.y = positions->position.y;
			particle->position.z = positions->position.z;
		}
		UnMapBuffer(pDebugPositionBuffer, deviceContext);

		_pAnnotation->EndEvent();

		bufferIsSwapped = !bufferIsSwapped;
	}

	_pAnnotation->EndEvent();
}

void SPH::RenderFluid()
{

}


void SPH::Draw()
{
	// Build Grid
	//BuildGrid();

	// Sort Grid Indices
	//SortGridIndices();

	// Build Grid Indices
	//BuildGridIndices();

	// Setup Particle Forces
	ParticleForcesSetup();

	// Render Fluid
	RenderFluid();
}