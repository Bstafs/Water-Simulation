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

	// Integrate Shaders
	if (FluidSimIntegrateShader) FluidSimIntegrateShader->Release();
	if (SpatialGridClearShader) SpatialGridClearShader->Release();
	if (SpatialGridAddParticleShader) SpatialGridAddParticleShader->Release();
	if (BitonicSortingShader) BitonicSortingShader->Release();
	if (RadixHistogramShader) RadixHistogramShader->Release();
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
	if (RadixSortConstantBuffer) RadixSortConstantBuffer->Release();
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
		Particle* newParticle = new Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 1.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), SMOOTHING_RADIUS, XMFLOAT3(0.0f, 0.0f, 0.0f));

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

	// Bitonic Sort
	BitonicSortingShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BitonicSort", device);

	// Radix Sort
	RadixHistogramShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixHistogram", device);
	RadixPreFixScanShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixScan", device);
	RadixPrepareOffsetsShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixPrepareOffsets", device);
	RadixScatterShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixScatter", device);

	GridOffsetsShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridOffsets", device);
	FluidSimCalculateDensity = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculateDensity", device);
	FluidSimCalculatePressure = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculatePressure", device);
	FluidSimIntegrateShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);

	// Constant Buffers
	SpatialGridConstantBuffer = CreateConstantBuffer(sizeof(SimulationParams), device, false);
	BitonicSortConstantBuffer = CreateConstantBuffer(sizeof(BitonicParams), device, false);
	RadixSortConstantBuffer = CreateConstantBuffer(sizeof(RadixParams), device, false);

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


	// Radix - Histogram UAV
	UINT numElements = threadGroupCountX * RADIX;

	D3D11_BUFFER_DESC outputDescRadixHistogram;
	outputDescRadixHistogram.Usage = D3D11_USAGE_DEFAULT;
	outputDescRadixHistogram.ByteWidth = numElements * sizeof(UINT);
	outputDescRadixHistogram.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDescRadixHistogram.CPUAccessFlags = 0;
	outputDescRadixHistogram.StructureByteStride = sizeof(UINT);
	outputDescRadixHistogram.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDescRadixHistogram, 0, &histogramBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDescRadixHistogram;
	uavDescRadixHistogram.Buffer.FirstElement = 0;
	uavDescRadixHistogram.Buffer.Flags = 0;
	uavDescRadixHistogram.Buffer.NumElements = numElements;
	uavDescRadixHistogram.Format = DXGI_FORMAT_UNKNOWN;
	uavDescRadixHistogram.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	hr = device->CreateUnorderedAccessView(histogramBuffer, &uavDescRadixHistogram, &histogramUAV);

	// Radix - Scan  GroupPref UAV
	D3D11_BUFFER_DESC outputDescRadixScanPrefs;
	outputDescRadixScanPrefs.Usage = D3D11_USAGE_DEFAULT;
	outputDescRadixScanPrefs.ByteWidth = numElements * sizeof(UINT);
	outputDescRadixScanPrefs.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDescRadixScanPrefs.CPUAccessFlags = 0;
	outputDescRadixScanPrefs.StructureByteStride = sizeof(UINT);
	outputDescRadixScanPrefs.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDescRadixScanPrefs, 0, &groupPrefBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDescRadixScanPrefs;
	uavDescRadixScanPrefs.Buffer.FirstElement = 0;
	uavDescRadixScanPrefs.Buffer.Flags = 0;
	uavDescRadixScanPrefs.Buffer.NumElements = numElements;
	uavDescRadixScanPrefs.Format = DXGI_FORMAT_UNKNOWN;
	uavDescRadixScanPrefs.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	hr = device->CreateUnorderedAccessView(groupPrefBuffer, &uavDescRadixScanPrefs, &groupPrefUAV);

	// Radix - Scan  Globals UAV
	D3D11_BUFFER_DESC outputDescRadixScanGlobals;
	outputDescRadixScanGlobals.Usage = D3D11_USAGE_DEFAULT;
	outputDescRadixScanGlobals.ByteWidth = (2 * RADIX) * sizeof(UINT);
	outputDescRadixScanGlobals.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDescRadixScanGlobals.CPUAccessFlags = 0;
	outputDescRadixScanGlobals.StructureByteStride = sizeof(UINT);
	outputDescRadixScanGlobals.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDescRadixScanGlobals, 0, &globalsBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDescRadixScanGlobals;
	uavDescRadixScanGlobals.Buffer.FirstElement = 0;
	uavDescRadixScanGlobals.Buffer.Flags = 0;
	uavDescRadixScanGlobals.Buffer.NumElements = 2 * RADIX;;
	uavDescRadixScanGlobals.Format = DXGI_FORMAT_UNKNOWN;
	uavDescRadixScanGlobals.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	hr = device->CreateUnorderedAccessView(globalsBuffer, &uavDescRadixScanGlobals, &globalsUAV);

	// Radix - Offsets
	D3D11_BUFFER_DESC outputDescRadixOffsets;
	outputDescRadixOffsets.Usage = D3D11_USAGE_DEFAULT;
	outputDescRadixOffsets.ByteWidth = numElements * sizeof(UINT);
	outputDescRadixOffsets.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDescRadixOffsets.CPUAccessFlags = 0;
	outputDescRadixOffsets.StructureByteStride = sizeof(UINT);
	outputDescRadixOffsets.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDescRadixOffsets, 0, &groupBaseBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDescRadixScanOffset;
	uavDescRadixScanOffset.Buffer.FirstElement = 0;
	uavDescRadixScanOffset.Buffer.Flags = 0;
	uavDescRadixScanOffset.Buffer.NumElements = numElements;
	uavDescRadixScanOffset.Format = DXGI_FORMAT_UNKNOWN;
	uavDescRadixScanOffset.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	hr = device->CreateUnorderedAccessView(groupBaseBuffer, &uavDescRadixScanOffset, &groupBaseUAV);
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

void SPH::UpdateRadixSorting(float deltaTime)
{
	RadixParams cb = {};
	cb.particleCount = NUM_OF_PARTICLES;
	cb.numGroups = threadGroupCountX;

	//ID3D11ShaderResourceView* curSRV = isBufferSwapped ? outputSRVSpatialGridB : outputSRVSpatialGridA;
	//ID3D11UnorderedAccessView* curUAV = isBufferSwapped ? outputUAVSpatialGridA : outputUAVSpatialGridB;


	for (UINT pass = 0; pass < 32 / RADIX_BITS; ++pass)
	{
		UINT currentBit = pass * RADIX_BITS;
		cb.currentBit = currentBit;
		deviceContext->UpdateSubresource(RadixSortConstantBuffer, 0, nullptr, &cb, 0, 0);

		// Histogram Phase
		deviceContext->CSSetShader(RadixHistogramShader, nullptr, 0);
		deviceContext->CSSetConstantBuffers(2, 1, &RadixSortConstantBuffer);

		deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
		//deviceContext->CSSetShaderResources(0, 1, &curSRV);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &histogramUAV, nullptr);

		deviceContext->Dispatch(threadGroupCountX, 1, 1);

		//deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		// Scan Phase
		deviceContext->CSSetShader(RadixPreFixScanShader, nullptr, 0);
		deviceContext->CSSetConstantBuffers(2, 1, &RadixSortConstantBuffer);

		deviceContext->CSSetUnorderedAccessViews(3, 1, &histogramUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(4, 1, &groupPrefUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &globalsUAV, nullptr);

		deviceContext->Dispatch(1, 1, 1);

		deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		// Prepare Offsets Phase
		deviceContext->CSSetShader(RadixPrepareOffsetsShader, nullptr, 0);
		deviceContext->CSSetConstantBuffers(2, 1, &RadixSortConstantBuffer);

		deviceContext->CSSetUnorderedAccessViews(4, 1, &groupPrefUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &globalsUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(6, 1, &groupBaseUAV, nullptr);

		deviceContext->Dispatch(1, 1, 1);

		deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(6, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		// Scatter Phase
		deviceContext->CSSetShader(RadixScatterShader, nullptr, 0);
		deviceContext->CSSetConstantBuffers(2, 1, &RadixSortConstantBuffer);

		//deviceContext->CSSetShaderResources(0, 1, &curSRV);
		//deviceContext->CSSetUnorderedAccessViews(1, 1, &curUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
		deviceContext->CSSetUnorderedAccessViews(6, 1, &groupBaseUAV, nullptr);

		deviceContext->Dispatch(threadGroupCountX, 1, 1);

		//deviceContext->CSSetShaderResources(0, 1, srvNull);
		deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(6, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		isBufferSwapped = !isBufferSwapped;
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

void SPH::Update(float deltaTime, float minX, float minZ)
{
	// SPH
	UpdateSpatialGridClear(deltaTime);
	UpdateAddParticlesToSpatialGrid(deltaTime);
	UpdateBitonicSorting(deltaTime);
	UpdateBuildGridOffsets(deltaTime);
	UpdateParticleDensities(deltaTime);
	UpdateParticlePressure(deltaTime);
	UpdateIntegrateComputeShader(deltaTime, minX, minZ);
}