#include "SPH.h"

SPH::SPH(ID3D11DeviceContext* contextdevice, ID3D11Device* device)
	:
	deviceContext(contextdevice),
	device(device)
{
	deviceContext = contextdevice;
	this->device = device;

	particleList.reserve(NUM_OF_PARTICLES);
	predictedPositions.resize(NUM_OF_PARTICLES);

	// Particle Initialization
	InitParticles();
	InitGPUResources();
}

SPH::~SPH()
{
	// Particle Cleanup
	for (auto particle : particleList)
	{
		delete particle;
		particle = nullptr;
	}
	particleList.clear();

	// Integrate Shader
	if (FluidSimIntegrateShader) FluidSimIntegrateShader->Release();
	if (SpatialGridClearShader) SpatialGridClearShader->Release();
	if (SpatialGridAddParticleShader) SpatialGridAddParticleShader->Release();
	if (FluidSimCalculateDensity) FluidSimCalculateDensity->Release();
	if (FluidSimCalculatePressure) FluidSimCalculatePressure->Release();


	if (inputBuffer) inputBuffer->Release();
	if (outputBuffer) outputBuffer->Release();
	if (outputResultBuffer) outputResultBuffer->Release();

	if (inputViewIntegrateA) inputViewIntegrateA->Release();
	if (outputUAVIntegrateA) outputUAVIntegrateA->Release();
	if (inputViewIntegrateB) inputViewIntegrateB->Release();
	if (outputUAVIntegrateB) outputUAVIntegrateB->Release();

	if (outputUAVSpatialGridA) outputUAVSpatialGridA->Release();
	if (outputUAVSpatialGridCountA) outputUAVSpatialGridCountA->Release();
	if (outputUAVSpatialGridB) outputUAVSpatialGridB->Release();
	if (outputUAVSpatialGridCountB) outputUAVSpatialGridCountB->Release();

	if (SpatialGridConstantBuffer) SpatialGridConstantBuffer->Release();
}

void SPH::InitParticles()
{
	// Particle Initialization
	float spacing = SMOOTHING_RADIUS;
	int particlesPerDimension = static_cast<int>(std::cbrt(NUM_OF_PARTICLES));

	float offsetX = -spacing * (particlesPerDimension - 1) / 2.0f;
	float offsetY = -spacing * (particlesPerDimension - 1) / 2.0f;
	float offsetZ = -spacing * (particlesPerDimension - 1) / 2.0f;

	for (int i = 0; i < NUM_OF_PARTICLES; i++)
	{
		Particle* newParticle = new Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 0.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), SMOOTHING_RADIUS, XMFLOAT3(0.0f, 0.0f, 0.0f));

		int xIndex = i % particlesPerDimension;
		int yIndex = (i / particlesPerDimension) % particlesPerDimension;
		int zIndex = i / (particlesPerDimension * particlesPerDimension);

		float x = offsetX + xIndex * spacing;
		float y = offsetY + yIndex * spacing;
		float z = offsetZ + zIndex * spacing;

		newParticle->position.x = x;
		newParticle->position.y = y;
		newParticle->position.z = z;

		particleList.emplace_back(newParticle);
	}
}

void SPH::InitGPUResources()
{
	HRESULT hr;


	// Shaders
	SpatialGridClearShader = CreateComputeShader(L"SPHComputeShader.hlsl", "ClearGrid", device);
	SpatialGridAddParticleShader = CreateComputeShader(L"SPHComputeShader.hlsl", "AddParticlesToGrid", device);
	BitonicSortingShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BitonicSort", device);
	GridOffsetsShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridOffsets", device);
	FluidSimCalculateDensity = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculateDensity", device);
	FluidSimCalculatePressure = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculatePressure", device);
	FluidSimIntegrateShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);

	// Constant Buffers
	SpatialGridConstantBuffer = CreateConstantBuffer(sizeof(SimulationParams), device, false);
	BitonicSortConstantBuffer = CreateConstantBuffer(sizeof(BitonicParams), device, true);


	// Structure Buffers
	std::vector<ParticleAttributes> position(NUM_OF_PARTICLES);
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* particle = particleList[i];

		position[i].position = particle->position;
		position[i].velocity = particle->velocity;
		position[i].density = particle->density;
		position[i].nearDensity = particle->nearDensity;
	}

	inputBuffer = CreateStructureBuffer(sizeof(ParticleAttributes), (float*)position.data(), NUM_OF_PARTICLES, device);


	// Spatial Grid
	D3D11_BUFFER_DESC outputDesc;
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.ByteWidth = (sizeof(unsigned int) * 3) * NUM_OF_PARTICLES;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.StructureByteStride = sizeof(unsigned int) * 3;
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDesc, 0, &SpatialGridOutputBuffer);

	outputDesc.Usage = D3D11_USAGE_STAGING;
	outputDesc.BindFlags = 0;
	outputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = device->CreateBuffer(&outputDesc, 0, &SpatialGridResultOutputBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = NUM_OF_PARTICLES;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	hr = device->CreateUnorderedAccessView(SpatialGridOutputBuffer, &uavDesc, &outputUAVSpatialGridA);
	hr = device->CreateUnorderedAccessView(SpatialGridOutputBuffer, &uavDesc, &outputUAVSpatialGridB);

	// Spatial Grid Count
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.ByteWidth = sizeof(unsigned int) * NUM_OF_PARTICLES;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.StructureByteStride = sizeof(unsigned int);
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDesc, 0, &SpatialGridOutputBufferCount);

	outputDesc.Usage = D3D11_USAGE_STAGING;
	outputDesc.BindFlags = 0;
	outputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = device->CreateBuffer(&outputDesc, 0, &SpatialGridResultOutputBufferCount);

	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = NUM_OF_PARTICLES;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	hr = device->CreateUnorderedAccessView(SpatialGridOutputBufferCount, &uavDesc, &outputUAVSpatialGridCountA);
	hr = device->CreateUnorderedAccessView(SpatialGridOutputBufferCount, &uavDesc, &outputUAVSpatialGridCountB);

	// UAV
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.ByteWidth = sizeof(ParticleAttributes) * NUM_OF_PARTICLES;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.StructureByteStride = sizeof(ParticleAttributes);
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDesc, 0, &outputBuffer);

	deviceContext->CopyResource(outputBuffer, inputBuffer);

	outputDesc.Usage = D3D11_USAGE_STAGING;
	outputDesc.BindFlags = 0;
	outputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = device->CreateBuffer(&outputDesc, 0, &outputResultBuffer);

	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = NUM_OF_PARTICLES;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUAVIntegrateA);
	hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUAVIntegrateB);
}

void SPH::UpdateSpatialGridClear(float deltaTime)
{
	// Bind compute shader and buffers (double buffer the grid)
	deviceContext->CSSetShader(SpatialGridClearShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);


	deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, &outputUAVSpatialGridCountA, nullptr);


	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind UAVs
	deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::UpdateAddParticlesToSpatialGrid(float deltaTime)
{
	SimulationParams cb = {};
	cb.numParticles = NUM_OF_PARTICLES;
	cb.minX = minX;
	cb.minZ = minZ;
	deviceContext->UpdateSubresource(SpatialGridConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Bind compute shader and resources
	deviceContext->CSSetShader(SpatialGridAddParticleShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAVIntegrateA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, &outputUAVSpatialGridCountA, nullptr);


	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind resources
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::UpdateBitonicSorting(float deltaTime)
{
	BitonicParams  params = {};
	params.numElements = NUM_OF_PARTICLES;
	deviceContext->UpdateSubresource(BitonicSortConstantBuffer, 0, nullptr, &params, 0, 0);

	for (UINT k = 2; k <= NUM_OF_PARTICLES; k <<= 1)
	{
		params.k = k;
		deviceContext->UpdateSubresource(BitonicSortConstantBuffer, 0, nullptr, &params, 0, 0);

		for (UINT j = k >> 1; j > 0; j >>= 1)
		{
			params.j = j;
			deviceContext->UpdateSubresource(BitonicSortConstantBuffer, 0, nullptr, &params, 0, 0);

			// Bind compute shader and resources
			deviceContext->CSSetShader(BitonicSortingShader, nullptr, 0);
			deviceContext->CSSetConstantBuffers(1, 1, &BitonicSortConstantBuffer);

			deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);

			// Dispatch compute shader
			deviceContext->Dispatch(threadGroupCountX, 1, 1);
			deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
			deviceContext->CSSetShader(nullptr, nullptr, 0);
		}
	}
}

void SPH::UpdateBuildGridOffsets(float deltaTime)
{
	BitonicParams cb = {};
	cb.numElements = NUM_OF_PARTICLES;
	deviceContext->UpdateSubresource(BitonicSortConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Bind compute shader and resources
	deviceContext->CSSetShader(GridOffsetsShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &BitonicSortConstantBuffer);


	deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, &outputUAVSpatialGridCountA, nullptr);


	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind resources
	deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::UpdateParticleDensities(float deltaTime)
{
	SimulationParams cb = {};
	cb.numParticles = NUM_OF_PARTICLES;
	cb.minX = minX;
	cb.minZ = minZ;
	deviceContext->UpdateSubresource(SpatialGridConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Bind compute shader and resources
	deviceContext->CSSetShader(FluidSimCalculateDensity, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);


	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAVIntegrateA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, &outputUAVSpatialGridCountA, nullptr);


	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind resources
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::UpdateParticlePressure(float deltaTime)
{
	SimulationParams cb = {};
	cb.numParticles = NUM_OF_PARTICLES;
	cb.minX = minX;
	cb.minZ = minZ;
	deviceContext->UpdateSubresource(SpatialGridConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Bind compute shader and resources
	deviceContext->CSSetShader(FluidSimCalculatePressure, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAVIntegrateA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, &outputUAVSpatialGridCountA, nullptr);

	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind resources
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::UpdateIntegrateComputeShader(float deltaTime, float minX, float minZ)
{
	SimulationParams cb = {};
	cb.numParticles = NUM_OF_PARTICLES;
	cb.minX = minX;
	cb.minZ = minZ;
	deviceContext->UpdateSubresource(SpatialGridConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Bind compute shader and resources
	deviceContext->CSSetShader(FluidSimIntegrateShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAVIntegrateA, nullptr);

	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind resources
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);


	// Copy result for CPU access
	deviceContext->CopyResource(outputResultBuffer, outputBuffer);

	// Map output buffer to apply results to particles
	D3D11_MAPPED_SUBRESOURCE mappedOutputResource;
	HRESULT	hr = deviceContext->Map(outputResultBuffer, 0, D3D11_MAP_READ, 0, &mappedOutputResource);
	if (SUCCEEDED(hr))
	{
		ParticleAttributes* outputData = reinterpret_cast<ParticleAttributes*>(mappedOutputResource.pData);
		for (int i = 0; i < particleList.size(); ++i)
		{
			Particle* particle = particleList[i];

			particle->position = outputData[i].position;
		}
		deviceContext->Unmap(outputResultBuffer, 0);
	}
}

void SPH::Update(float deltaTime, float minX, float minZ)
{
	// GPU Side
	UpdateSpatialGridClear(deltaTime);
	UpdateAddParticlesToSpatialGrid(deltaTime);
	UpdateBitonicSorting(deltaTime);
	UpdateBuildGridOffsets(deltaTime);
	UpdateParticleDensities(deltaTime);
	UpdateParticlePressure(deltaTime);
	UpdateIntegrateComputeShader(deltaTime, minX, minZ);

	isBufferSwapped = !isBufferSwapped;
}