#include "SPH.h"

SPH::SPH(ID3D11DeviceContext* contextdevice, ID3D11Device* device)
	:
	deviceContext(contextdevice),
	device(device)
{
	deviceContext->QueryInterface(
		__uuidof(ID3DUserDefinedAnnotation),
		(void**)&g_Annotation
	);


	particleList.reserve(NUM_OF_PARTICLES);
	predictedPositions.resize(NUM_OF_PARTICLES);

	// Particle Initialization
	InitParticles();
	InitGPUResources();
}

SPH::~SPH()
{
	// Particle Cleanup
	particleList.clear();

	// Integrate Shaders
	if (FluidSimIntegrateShader) FluidSimIntegrateShader->Release();
	if (SpatialGridClearShader) SpatialGridClearShader->Release();
	if (SpatialGridAddParticleShader) SpatialGridAddParticleShader->Release();
	if (BitonicSortingShader) BitonicSortingShader->Release();
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
		Particle newParticle = Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 1.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), SMOOTHING_RADIUS, XMFLOAT3(0.0f, 0.0f, 0.0f));

		int xIndex = i % particlesPerDimension;
		int yIndex = (i / particlesPerDimension) % particlesPerDimension;
		int zIndex = i / (particlesPerDimension * particlesPerDimension);

		float x = offsetX + xIndex * spacing;
		float y = offsetY + yIndex * spacing;
		float z = offsetZ + zIndex * spacing;

		newParticle.position.x = x;
		newParticle.position.y = y;
		newParticle.position.z = z;

		particleList.emplace_back(newParticle);
	}
}

void SPH::InitGPUResources()
{
	HRESULT hr;

	// Shaders
	SpatialGridClearShader = CreateComputeShader(L"SPHComputeShader.hlsl", "ClearGrid", device);
	SpatialGridAddParticleShader = CreateComputeShader(L"SPHComputeShader.hlsl", "AddParticlesToGrid", device);

	// Bitonic Sort
	BitonicSortingShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BitonicSort", device);

	GridOffsetsShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridOffsets", device);
	FluidSimCalculateDensity = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculateDensity", device);
	FluidSimCalculatePressure = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculatePressure", device);
	FluidSimIntegrateShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	MarchingCubesShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildDensityGrid", device);

	// Constant Buffers
	SpatialGridConstantBuffer = CreateConstantBuffer(sizeof(SimulationParams), device, false);
	BitonicSortConstantBuffer = CreateConstantBuffer(sizeof(BitonicParams), device, false);
	MCConstantBuffer = CreateConstantBuffer(sizeof(MCGridParams), device, false);

	// Structure Buffers
	std::vector<ParticleAttributes> position(NUM_OF_PARTICLES);
	for (int i = 0; i < particleList.size(); i++)
	{
		Particle particle = particleList[i];

		position[i].position =	    particle.position;
		position[i].velocity =		particle.velocity;
		position[i].density =		particle.density;
		position[i].nearDensity =	particle.nearDensity;
	}

	inputBuffer = CreateStructureBuffer(sizeof(ParticleAttributes), (float*)position.data(), NUM_OF_PARTICLES, device);

	// Position GPU Buffer (float4 per particle)
	D3D11_BUFFER_DESC descPositions = {};
	descPositions.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	descPositions.Usage = D3D11_USAGE_DEFAULT;     
	descPositions.CPUAccessFlags = 0;
	descPositions.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	descPositions.StructureByteStride = sizeof(XMFLOAT4);
	descPositions.ByteWidth = descPositions.StructureByteStride * NUM_OF_PARTICLES;
	hr = device->CreateBuffer(&descPositions, nullptr, &g_pParticlePositionBuffer);

	// Create UAV
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDescPositions = {};
	uavDescPositions.Format = DXGI_FORMAT_UNKNOWN;
	uavDescPositions.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDescPositions.Buffer.NumElements = NUM_OF_PARTICLES;
	device->CreateUnorderedAccessView(g_pParticlePositionBuffer, &uavDescPositions, &g_pParticlePositionUAV);

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDescPositions = {};
	srvDescPositions.Format = DXGI_FORMAT_UNKNOWN;
	srvDescPositions.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDescPositions.Buffer.NumElements = NUM_OF_PARTICLES;
	device->CreateShaderResourceView(g_pParticlePositionBuffer, &srvDescPositions, &g_pParticlePositionSRV);

	// Spatial Grid
	UINT elementCount = NUM_OF_PARTICLES;
	UINT stride = (sizeof(unsigned int) * 3); // 16
	UINT byteWidth = elementCount * stride;

	// Spatial Grid Output Buffers
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = byteWidth;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = stride;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;


	device->CreateBuffer(&desc, 0, &SpatialGridOutputBufferA);
	device->CreateBuffer(&desc, 0, &SpatialGridOutputBufferB);

	// Spatial Grid UAVs
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = elementCount;
	uavDesc.Buffer.Flags = 0;

	hr = device->CreateUnorderedAccessView(SpatialGridOutputBufferA, &uavDesc, &outputUAVSpatialGridA);
	hr = device->CreateUnorderedAccessView(SpatialGridOutputBufferB, &uavDesc, &outputUAVSpatialGridB);

	// Spatial Grid SRVs
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = elementCount;
	hr = device->CreateShaderResourceView(SpatialGridOutputBufferA, &srvDesc, &outputSRVSpatialGridA);
	hr = device->CreateShaderResourceView(SpatialGridOutputBufferB, &srvDesc, &outputSRVSpatialGridB);


	// Spatial Grid Count
	D3D11_BUFFER_DESC outputDesc;
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

	// Marching Cubes

	D3D11_BUFFER_DESC mcDesc = {};
	mcDesc.ByteWidth = sizeof(float) * VOXEL_COUNT;
	mcDesc.Usage = D3D11_USAGE_DEFAULT;
	mcDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	mcDesc.StructureByteStride = sizeof(float);
	mcDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	device->CreateBuffer(&desc, nullptr, &voxelBuffer);
	device->CreateUnorderedAccessView(voxelBuffer, nullptr, &voxelUAV);
	device->CreateShaderResourceView(voxelBuffer, nullptr, &voxelSRV);
}

void SPH::UpdateSpatialGridClear(float deltaTime)
{
	// Bind compute shader and buffers (double buffer the grid)
	deviceContext->CSSetShader(SpatialGridClearShader, nullptr, 0);

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
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);
	deviceContext->CSSetConstantBuffers(1, 1, &BitonicSortConstantBuffer);


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
	cb.deltaTime = deltaTime;
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
	cb.deltaTime = deltaTime;
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
	cb.deltaTime = deltaTime;
	deviceContext->UpdateSubresource(SpatialGridConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Bind compute shader and resources
	deviceContext->CSSetShader(FluidSimIntegrateShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAVIntegrateA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(7, 1, &g_pParticlePositionUAV, nullptr);

	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind resources
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(7, 1, uavViewNull, nullptr);

	// Unbind compute shader
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::UpdateMarchingCubes()
{
	MCGridParams cb = {};
	cb.gridSizeX = gridSizeX;
	cb.gridSizeY = gridSizeY;
	cb.gridSizeZ = gridSizeZ;
	cb.voxelSize = voxelSize;
	deviceContext->UpdateSubresource(MCConstantBuffer, 0, nullptr, &cb, 0, 0);

	deviceContext->CSSetShader(MarchingCubesShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(2, 1, &MCConstantBuffer);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAVIntegrateA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(3, 1, &voxelUAV, nullptr);

	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::Update(float deltaTime, float minX, float minZ)
{
	// SPH
	UpdateSpatialGridClear(deltaTime);
	UpdateAddParticlesToSpatialGrid(deltaTime);
	UpdateBitonicSorting(deltaTime);
	UpdateBuildGridOffsets(deltaTime);
	UpdateParticleDensities(deltaTime);

	if (g_Annotation)
		g_Annotation->BeginEvent(L"SPH Pressure Pass");
	UpdateParticlePressure(deltaTime);
		if (g_Annotation)
		g_Annotation->EndEvent();

	UpdateIntegrateComputeShader(deltaTime, minX, minZ);

	UpdateMarchingCubes();
}