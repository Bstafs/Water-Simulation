#include "SPH.h"

// Numthreads size for the sort
const UINT BITONIC_BLOCK_SIZE = 512;
const UINT TRANSPOSE_BLOCK_SIZE = 8;
const UINT NUM_GRID_INDICES = 65536;
//walls
float particleSpacing = 5.0f;

constexpr float mapZ = 1.0f;
constexpr float mapX = 1.0f;
constexpr float mapY = 1.0f;

const float low_wall = 1;
const float left_wall = 1.5;
const float near_wall = 32 * particleSpacing;

SPH::SPH(int numbParticles, float mass, float density, float viscosity, float h, float g, float elasticity, float pressure, ID3D11DeviceContext* contextdevice, ID3D11Device* device)
{
	// Variable Initialization
	numberOfParticles = numbParticles;
	sphDensity = density;
	sphViscosity = viscosity;
	sphH = h;
	sphG = g;
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
	if (pIntegrateBufferOne) pIntegrateBufferOne->Release();
	if (pIntegrateUAVOne) pIntegrateUAVOne->Release();
	if (pIntegrateSRVOne) pIntegrateSRVOne->Release();
	if (pIntegrateBufferTwo) pIntegrateBufferTwo->Release();
	if (pIntegrateUAVTwo) pIntegrateUAVTwo->Release();
	if (pIntegrateSRVTwo) pIntegrateSRVTwo->Release();

	if (pParticleDensityCS) pParticleDensityCS->Release();
	if (pDensitySRV) pDensitySRV->Release();
	if (pDensityUAV) pDensityUAV->Release();
	if (pDensityBuffer) pDensityBuffer->Release();

	if (pParticleForcesCS) pParticleForcesCS->Release();
	if (pForcesSRV) pForcesSRV->Release();
	if (pForcesUAV) pForcesUAV->Release();
	if (pForcesBuffer) pForcesBuffer->Release();

	if (pParticleGridCS) pParticleGridCS->Release();
	if (pGridSRV) pGridSRV->Release();
	if (pGridUAV) pGridUAV->Release();
	if (pGridBuffer) pGridBuffer->Release();

	if (pParticleClearGridIndicesCS)pParticleClearGridIndicesCS->Release();

	if (pParticleGridIndicesCS)pParticleGridIndicesCS->Release();
	if (pGridIndicesBufferOne)pGridIndicesBufferOne->Release();
	if (pGridIndicesSRVOne)pGridIndicesSRVOne->Release();
	if (pGridIndicesUAVOne) pGridIndicesUAVOne->Release();

	if (pGridIndicesTempBuffer)pGridIndicesTempBuffer->Release();
	if (pGridIndicesTempSRV)pGridIndicesTempSRV->Release();
	if (pGridIndicesTempUAV)pGridIndicesTempUAV->Release();

	if (pTransposeMatrixCS) pTransposeMatrixCS->Release();
}

void SPH::InitParticles()
{
	// Following Realtime Particle I researched, I need to loop over every particle for x,y,z
	srand(10);

	for (int i = 0; i < numberOfParticles; ++i)
	{
		float bias = i / 50000.0f;

		float x = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH / 10.0f;
		float y = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH / 10.0f;
		float z = (float(rand()) / float((RAND_MAX)) * 0.5f - 1.0f) * sphH / 10.0f;

		XMFLOAT3 startingParticlePosition = XMFLOAT3(x * particleSpacing + bias, y * particleSpacing + bias, z * particleSpacing + bias);

		Particle* newParticle = new Particle(MASS_CONSTANT, 0.1f, startingParticlePosition, XMFLOAT3(1.0f, 1.0f, 1.0f));

		newParticle->elasticity = sphElasticity;
		newParticle->acceleration = XMFLOAT3(1.0f, 1.0f, 1.0f);
		newParticle->velocity = XMFLOAT3(1.0f, 1.0f, 1.0f);
		newParticle->force = XMFLOAT3(1.0f, 1.0f, 1.0f);
		newParticle->density = sphDensity;
		newParticle->mass = MASS_CONSTANT;

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

		integrateData[i].position.x = particleList[i]->position.x;
		integrateData[i].position.y = particleList[i]->position.y;
		integrateData[i].position.z = particleList[i]->position.z;
									
		integrateData[i].velocity.x = particleList[i]->velocity.x;
		integrateData[i].velocity.y = particleList[i]->velocity.y;
		integrateData[i].velocity.z = particleList[i]->velocity.z;

		integrateData[i].padding01 = 0.0f;
		integrateData[i].padding02 = 0.0f;
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
		forceData[i].acceleration.x = particleList[i]->acceleration.x;
		forceData[i].acceleration.y = particleList[i]->acceleration.y;
		forceData[i].acceleration.z = particleList[i]->acceleration.z;

		forceData[i].padding01 = 0.0f;
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
		densityData[i].density = particleList[i]->density;
		densityData[i].padding01 = XMFLOAT3(0.0f, 0.0f, 0.0f);
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

	//--Build Grid (Double Buffer)
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

	pGridBufferTwo = CreateStructureBuffer(sizeof(GridKeyStructure), nullptr, numberOfParticles, device);
	SetDebugName(pGridBufferTwo, "Particle Grid Buffer 2");

	pGridUAVTwo = CreateUnorderedAccessView(pGridBufferTwo, numberOfParticles, device);
	SetDebugName(pGridUAVTwo, "Particle Grid UAV 2");

	pGridSRVTwo = CreateShaderResourceView(pGridBufferTwo, numberOfParticles, device);
	SetDebugName(pGridSRVTwo, "Particle Grid SRV 2");


	//--Build Grid Sorter
	pGridSorterShader = CreateComputeShader(L"SPHComputeShader.hlsl", "SortGridIndices", device);

	//--Build Grid Indices
	pParticleGridIndicesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridIndicesCS", device);
	SetDebugName(pParticleGridIndicesCS, "Particle Grid Indices Shader");

	// Double Buffer
	pGridIndicesBufferOne = CreateStructureBuffer(sizeof(GridBorderStructure), nullptr, GRID_DIMENSION, device);
	SetDebugName(pGridIndicesBufferOne, "Particle Grid Indices Buffer One");

	pGridIndicesUAVOne = CreateUnorderedAccessView(pGridIndicesBufferOne, GRID_DIMENSION, device);
	SetDebugName(pGridIndicesUAVOne, "Particle Grid Indices UAV One");

	pGridIndicesSRVOne = CreateShaderResourceView(pGridIndicesBufferOne, GRID_DIMENSION, device);
	SetDebugName(pGridIndicesSRVOne, "Particle Grid Indices SRV One");

	pGridIndicesBufferTwo = CreateStructureBuffer(sizeof(GridBorderStructure), nullptr, GRID_DIMENSION, device);
	SetDebugName(pGridIndicesBufferTwo, "Particle Grid Indices Buffer Two");

	pGridIndicesUAVTwo = CreateUnorderedAccessView(pGridIndicesBufferTwo, GRID_DIMENSION, device);
	SetDebugName(pGridIndicesUAVTwo, "Particle Grid Indices UAV Two");

	pGridIndicesSRVTwo = CreateShaderResourceView(pGridIndicesBufferTwo, GRID_DIMENSION, device);
	SetDebugName(pGridIndicesSRVTwo, "Particle Grid Indices SRV Two");


	pDebugGridIndicesBuffer = CreateReadableStructureBuffer(sizeof(GridKeyStructure) * GRID_DIMENSION, nullptr, device);
	SetDebugName(pDebugGridIndicesBuffer, "Particle Grid Indices Readable Structure Buffer");

	// Transpose Sort Matrix
	pTransposeMatrixCS = CreateComputeShader(L"SPHComputeShader.hlsl", "TransposeMatrixCS", device);
	SetDebugName(pTransposeMatrixCS, "Transpose Matrix Shader");

	pGridIndicesTempBuffer = CreateStructureBuffer(sizeof(GridKeyStructure), nullptr, numberOfParticles, device);
	SetDebugName(pGridIndicesTempBuffer, "Transpose Matrix Indices Temp Buffer");

	pGridIndicesTempUAV = CreateUnorderedAccessView(pGridIndicesTempBuffer, numberOfParticles, device);
	SetDebugName(pGridIndicesTempUAV, "Transpose Matrix Temp UAV");

	pGridIndicesTempSRV = CreateShaderResourceView(pGridIndicesTempBuffer, numberOfParticles, device);
	SetDebugName(pGridIndicesTempSRV, "Transpose Matrix Temp SRV");

	//--Clear Grid Indices
	pParticleClearGridIndicesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "ClearGridIndicesCS", device);
	SetDebugName(pParticleClearGridIndicesCS, "Particle Grid Clear Indices Shader");

	//--Rearrange Particles
	pRearrangeParticlesCS = CreateComputeShader(L"SPHComputeShader.hlsl", "RearrangeParticlesCS", device);
	SetDebugName(pRearrangeParticlesCS, "Rearrange Particle Shader");
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
	//ParticleBoxCollision();
}

void SPH::BuildGrid()
{
	deviceContext->Flush();
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Build Grid");


	deviceContext->CSSetShader(pParticleGridCS, nullptr, 0);

	deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

	if (bufferIsSwapped == false)
	{
		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVOne);
	}
	else
	{
		deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVTwo);
	}

	if (isBufferSwappedGrid == false)
	{
		deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);
	}
	else
	{
		deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, nullptr);
	}

	deviceContext->Dispatch(numberOfParticles / GRID_DIMENSION, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(0, 1, srvNull);
	deviceContext->CSSetShaderResources(3, 1, srvNull);

	_pAnnotation->EndEvent();
}

void SPH::SortGrid()
{
	// Bitonic Sort

	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Bitonic Sort");

	deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);

	const UINT NUM_ELEMENTS = numberOfParticles;
	const UINT MATRIX_WIDTH = BITONIC_BLOCK_SIZE;
	const UINT MATRIX_HEIGHT = NUM_ELEMENTS / BITONIC_BLOCK_SIZE;

	_pAnnotation->BeginEvent(L"Particle Sort");
	// Sort the data
	// First sort the rows for the levels <= to the block size
	for (UINT level = 2; level <= BITONIC_BLOCK_SIZE; level <<= 1)
	{
		sortConstantCPUBuffer.sortLevel = level;
		sortConstantCPUBuffer.sortAlternateMask = level;
		sortConstantCPUBuffer.iWidth = MATRIX_WIDTH;
		sortConstantCPUBuffer.iHeight = MATRIX_HEIGHT;
		sortConstantCPUBuffer.padding01 = XMFLOAT4(1, 1, 1, 1);
		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);
		deviceContext->CSSetConstantBuffers(2, 1, &pSortConstantBuffer);

		UINT InitialCounts = 0;

		if (isBufferSwappedGrid == false)
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, &InitialCounts);
		}
		else
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, &InitialCounts);
		}

		deviceContext->Dispatch(NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1);
	}

	deviceContext->CSSetShaderResources(3, 1, srvNull);
	deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);

	_pAnnotation->EndEvent();

	_pAnnotation->BeginEvent(L"Particle Transpose");

	// Then sort the rows and columns for the levels > than the block size
   // Transpose. Sort the Columns. Transpose. Sort the Rows.

	for (UINT level = (BITONIC_BLOCK_SIZE << 1); level <= NUM_ELEMENTS; level <<= 1)
	{
		// Transpose Data from buffer 1 to buffer 2
		sortConstantCPUBuffer.sortLevel = level / BITONIC_BLOCK_SIZE;
		sortConstantCPUBuffer.sortAlternateMask = (level & ~NUM_ELEMENTS) / BITONIC_BLOCK_SIZE;
		sortConstantCPUBuffer.iWidth = MATRIX_WIDTH;
		sortConstantCPUBuffer.iHeight = MATRIX_HEIGHT;
		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);

		UINT InitialCounts = 0;

		deviceContext->CSSetShader(pTransposeMatrixCS, nullptr, 0);

		if (isBufferSwappedGrid == false)
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, &InitialCounts);
		}
		else
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, &InitialCounts);
		}
		deviceContext->Dispatch(MATRIX_WIDTH, MATRIX_HEIGHT, 1);

		// Sort the transposed Column Data
		deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);
		deviceContext->Dispatch(NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1);

		sortConstantCPUBuffer.sortLevel = BITONIC_BLOCK_SIZE;
		sortConstantCPUBuffer.sortAlternateMask = level;
		sortConstantCPUBuffer.iWidth = MATRIX_HEIGHT;
		sortConstantCPUBuffer.iHeight = MATRIX_WIDTH;
		UpdateBuffer((float*)&sortConstantCPUBuffer, sizeof(SortConstantBuffer), pSortConstantBuffer, deviceContext);

		//deviceContext->CSSetShaderResources(3, 1, srvNull);

		// Transpose the data from buffer 2 back to buffer 1
		deviceContext->CSSetShader(pTransposeMatrixCS, nullptr, 0);
		if (isBufferSwappedGrid == false)
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, &InitialCounts);
		}
		else
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, &InitialCounts);
		}

		deviceContext->Dispatch(MATRIX_HEIGHT, MATRIX_WIDTH, 1);

		// Sort the rows Data
		deviceContext->CSSetShader(pGridSorterShader, nullptr, 0);

		if (isBufferSwappedGrid == false)
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, &InitialCounts);
		}
		else
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, &InitialCounts);
		}

		deviceContext->Dispatch(NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1);
	}

	//deviceContext->CSSetShader(nullptr, nullptr, 0);
	//deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	//deviceContext->CSSetShaderResources(3, 1, srvNull);

	_pAnnotation->EndEvent();

	_pAnnotation->EndEvent();
}

void SPH::ClearGridIndices()
{
	// Clear
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Clear Indices");

	deviceContext->Flush();
	deviceContext->CSSetShader(pParticleClearGridIndicesCS, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAVOne, nullptr);
	deviceContext->Dispatch(NUM_GRID_INDICES / GRID_DIMENSION, 1, 1);

	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);

	_pAnnotation->EndEvent();
}

void SPH::BuildGridIndices()
{
	// Build
	deviceContext->Flush();

	_pAnnotation->BeginEvent(L"Particle Build Indices");

	deviceContext->CSSetShader(pParticleGridIndicesCS, nullptr, 0);

	if (isBufferSwappedGrid == false)
	{
		deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, nullptr);
	}
	else
	{
		deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);
	}

	if (isBufferSwappedIndices == false)
	{
		deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRVOne);
		deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAVTwo, nullptr);
	}
	else
	{
		deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRVTwo);
		deviceContext->CSSetUnorderedAccessViews(4, 1, &pGridIndicesUAVOne, nullptr);
	}

	deviceContext->Dispatch(NUM_GRID_INDICES / GRID_DIMENSION, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);

	deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(3, 1, srvNull);
	deviceContext->CSSetShaderResources(4, 1, srvNull);

	_pAnnotation->EndEvent();
	deviceContext->Flush();
}


void SPH::SetUpParticleConstantBuffer()
{
	particleConstantCPUBuffer.particleCount = numberOfParticles;
	particleConstantCPUBuffer.wallStiffness = 1000.0f;
	particleConstantCPUBuffer.padding00 = XMFLOAT2(0.0f, 0.0f);
	particleConstantCPUBuffer.deltaTime = 1.0f / 60.0f;
	particleConstantCPUBuffer.smoothingLength = sphH;
	particleConstantCPUBuffer.pressure = 200.0f;
	particleConstantCPUBuffer.restDensity = sphDensity;
	particleConstantCPUBuffer.densityCoef = MASS_CONSTANT * 315.0f / (64.0f * PI * powf(sphH, 9));;
	particleConstantCPUBuffer.GradPressureCoef = MASS_CONSTANT * -45.0f / (PI * powf(sphH, 6));;
	particleConstantCPUBuffer.LapViscosityCoef = MASS_CONSTANT * sphViscosity * 45.0f / (PI * powf(sphH, 6));;
	particleConstantCPUBuffer.gravity = sphG;

	particleConstantCPUBuffer.vPlanes[0] = XMFLOAT4(0.0f, 0.0f, 1.0f, low_wall); // Back
	particleConstantCPUBuffer.vPlanes[1] = XMFLOAT4(0.0f, 0.0f, -1.0f, low_wall); // Front

	particleConstantCPUBuffer.vPlanes[2] = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f); // Bottom Starts at 0.0f
	particleConstantCPUBuffer.vPlanes[3] = XMFLOAT4(0.0f, -1.0f, 0.0f, 32.0f); // Top


	particleConstantCPUBuffer.vPlanes[4] = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f); // Left
	particleConstantCPUBuffer.vPlanes[5] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 32.0f); // Right


	particleConstantCPUBuffer.gridDim.x = 0.5f / sphH;
	particleConstantCPUBuffer.gridDim.y = 0.5f / sphH;
	particleConstantCPUBuffer.gridDim.z = 0.5f / sphH;
	particleConstantCPUBuffer.gridDim.w = 0.0f;


	UpdateBuffer((float*)&particleConstantCPUBuffer, sizeof(ParticleConstantBuffer), pParticleConstantBuffer, deviceContext);
}

void SPH::RearrangeParticles()
{
	if (!_pAnnotation && deviceContext)
		deviceContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Particle Rearrange");

	deviceContext->CSSetShader(pRearrangeParticlesCS, nullptr, 0);

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

	if (isBufferSwappedGrid == false)
	{
		deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
	}
	else
	{
		deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
	}

	deviceContext->Dispatch(numberOfParticles / GRID_DIMENSION, 1, 1);

	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
	deviceContext->CSSetShaderResources(0, 1, srvNull);
	deviceContext->CSSetShaderResources(3, 1, srvNull);

	_pAnnotation->EndEvent();
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

		if (isBufferSwappedGrid == false)
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
		}
		else
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
		}

		if (isBufferSwappedIndices == false)
		{
			deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRVOne);
		}
		else
		{
			deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRVTwo);
		}

		ID3D11UnorderedAccessView* cleanerUAV = nullptr;
		deviceContext->CSSetUnorderedAccessViews(2, 1, &cleanerUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(2, 1, &pDensityUAV, nullptr);

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(numberOfParticles, 1, 1);

		deviceContext->CopyResource(pDebugDensityBuffer, pDensityBuffer);
		ParticleDensity* densities = reinterpret_cast<ParticleDensity*>(MapBuffer(pDebugDensityBuffer, deviceContext));
		for (int i = 0; i < numberOfParticles; ++i)
		{
			particleList[i]->density = densities[i].density;
		}
		UnMapBuffer(pDebugDensityBuffer, deviceContext);

		deviceContext->CSSetShader(nullptr, nullptr, 0);

		deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

		deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetShaderResources(3, 1, srvNull);
		deviceContext->CSSetShaderResources(4, 1, srvNull);

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

		if (isBufferSwappedGrid == false)
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRV);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAVTwo, nullptr);
		}
		else
		{
			deviceContext->CSSetShaderResources(3, 1, &pGridSRVTwo);
			deviceContext->CSSetUnorderedAccessViews(3, 1, &pGridUAV, nullptr);
		}

		if (isBufferSwappedIndices == false)
		{
			deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRVOne);
		}
		else
		{
			deviceContext->CSSetShaderResources(4, 1, &pGridIndicesSRVTwo);
		}

		ID3D11UnorderedAccessView* cleanerUAV = nullptr;
		deviceContext->CSSetUnorderedAccessViews(1, 1, &cleanerUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(1, 1, &pForcesUAV, nullptr);

		deviceContext->CSSetConstantBuffers(1, 1, &pParticleConstantBuffer);

		deviceContext->Dispatch(numberOfParticles, 1, 1);

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
			particleList[i]->acceleration = forces[i].acceleration;
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
			particleList[i]->velocity.x = positions[i].velocity.x;
			particleList[i]->velocity.y = positions[i].velocity.y;
			particleList[i]->velocity.z = positions[i].velocity.z;
	
			particleList[i]->position.x = positions[i].position.x;
			particleList[i]->position.y = positions[i].position.y;
			particleList[i]->position.z = positions[i].position.z;
		}
		UnMapBuffer(pDebugPositionBuffer, deviceContext);

		_pAnnotation->EndEvent();

		bufferIsSwapped = !bufferIsSwapped;
		isBufferSwappedGrid = !isBufferSwappedGrid;
		isBufferSwappedIndices = !isBufferSwappedIndices;
	}

	_pAnnotation->EndEvent();
}

void SPH::RenderFluid()
{

}


void SPH::Draw()
{
	// Build Grid 
	BuildGrid();

	// Sort Grid 
	SortGrid();

	// Clear Indices 
	ClearGridIndices();

	// Build Grid Indices
	BuildGridIndices();

	// Rearrange
	RearrangeParticles();

	// Setup Particle Forces
	ParticleForcesSetup();

	// Render Fluid
	RenderFluid();
}