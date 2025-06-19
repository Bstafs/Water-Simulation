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

	//marchingCubes = make_unique<MarchingCubes>();


	//scalarField = marchingCubes->GenerateScalarField(particleList, gridSizeX, gridSizeY, gridSizeZ, cellSize, 
	//	smoothingRadius, isoLevel, gridOrigin);

	//float minVal = FLT_MAX;
	//float maxVal = -FLT_MAX;

	//for (float val : scalarField) {
	//	if (val < minVal) minVal = val;
	//	if (val > maxVal) maxVal = val;
	//}

	//char buffer[128];
	//sprintf_s(buffer, "ScalarField Range: Min = %f, Max = %f\n", minVal, maxVal);
	//OutputDebugStringA(buffer);

	//int zeroCount = 0;
	//int nonZeroCount = 0;

	//for (float val : scalarField) {
	//	if (val == 0.0f) zeroCount++;
	//	else nonZeroCount++;
	//}

	//std::string debugStr = "Scalar Field: " + std::to_string(zeroCount) +
	//	" zeros, " + std::to_string(nonZeroCount) + " non-zeros\n";

	//OutputDebugStringA(debugStr.c_str());


	//marchingCubes->GenerateMarchingCubesMesh(
	//	scalarField,
	//	gridSizeX, gridSizeY, gridSizeZ,
	//	cellSize,
	//	isoLevel,
	//	MarchingCubesVertices,
	//	MarchingCubesIndices
	//);

	//sprintf_s(buffer, "Generated Vertices: %zu, Indices: %zu\n", MarchingCubesVertices.size(), MarchingCubesIndices.size());
	//OutputDebugStringA(buffer);

	//D3D11_BUFFER_DESC vbDesc, ibDesc;
	//D3D11_SUBRESOURCE_DATA vbData, ibData;

	//// Vertex Marching Cubes
	//vbDesc.Usage = D3D11_USAGE_DEFAULT;
	//vbDesc.ByteWidth = sizeof(SimpleVertex) * MarchingCubesVertices.size();
	//vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	//vbDesc.CPUAccessFlags = 0;
	//vbDesc.MiscFlags = 0;
	//vbData.pSysMem = MarchingCubesVertices.data();
	//device->CreateBuffer(&vbDesc, &vbData, &_pVertexBufferMarchingCubes);

	//// Index Buffer Marching Cubes
	//ibDesc.Usage = D3D11_USAGE_DEFAULT;
	//ibDesc.ByteWidth = sizeof(DWORD) * MarchingCubesIndices.size();
	//ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	//ibDesc.CPUAccessFlags = 0;
	//ibDesc.MiscFlags = 0;
	//ibData.pSysMem = MarchingCubesIndices.data();
	//device->CreateBuffer(&ibDesc, &ibData, &_pIndexBufferMarchingCubes);
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
	BitonicSortingShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BitonicSort", device);
	RadixHistogramShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixHistogram", device);
	RadixPreFixSumShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixPrefixSum", device);
	RadixPrepareOffsetsShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixPrepareOffsets", device);
	GridOffsetsShader = CreateComputeShader(L"SPHComputeShader.hlsl", "BuildGridOffsets", device);
	RadixScatterShader = CreateComputeShader(L"SPHComputeShader.hlsl", "RadixScatter", device);
	FluidSimCalculateDensity = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculateDensity", device);
	FluidSimCalculatePressure = CreateComputeShader(L"SPHComputeShader.hlsl", "CalculatePressure", device);
	FluidSimIntegrateShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	MarchingCubesShader = CreateComputeShader(L"SPHComputeShader.hlsl", "MarchingCubesCS", device);

	// Constant Buffers
	SpatialGridConstantBuffer = CreateConstantBuffer(sizeof(SimulationParams), device, false);
	BitonicSortConstantBuffer = CreateConstantBuffer(sizeof(BitonicParams), device, false);


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


	// Radix - Histogram Buffer & UAV

	// Buffer Description
	D3D11_BUFFER_DESC histogramDesc;
	ZeroMemory(&histogramDesc, sizeof(histogramDesc));
	histogramDesc.Usage = D3D11_USAGE_DEFAULT;
	histogramDesc.ByteWidth = sizeof(UINT) * RADIX * threadGroupCountX; 
	histogramDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	histogramDesc.CPUAccessFlags = 0;
	histogramDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	histogramDesc.StructureByteStride = sizeof(UINT); 
	hr = device->CreateBuffer(&histogramDesc, nullptr, &histogramBuffer);

	// UAV Description
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = RADIX * threadGroupCountX;
	uavDesc.Buffer.Flags = 0;
	hr = device->CreateUnorderedAccessView(histogramBuffer, &uavDesc, &histogramUAV);

	// Radix - Global Params Buffer & UAV
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeof(UINT) * 4; // Matches your [currentBit, passCount, 0, 0]
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(UINT);

	// UAV Description
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 4; // Matches your 4-element array

	// Create buffer and UAV
	hr = device->CreateBuffer(&desc, nullptr, &globalParamsBuffer);

	hr = device->CreateUnorderedAccessView(globalParamsBuffer, &uavDesc, &globalParamsUAV);

	// Radix - PreFix Sum Buffer & UAV
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeof(UINT) * RADIX;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(UINT);
	hr = device->CreateBuffer(&desc, nullptr, &preFixSumBuffer);

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = RADIX;
	hr = device->CreateUnorderedAccessView(preFixSumBuffer, &uavDesc, &preFixSumUAV);

	// Radix - SortedIndices Buffer & UAV
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = (sizeof(unsigned int) * 3) * NUM_OF_PARTICLES;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = (sizeof(unsigned int) * 3);
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&desc, nullptr, &sortedIndicesBuffer);

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = NUM_OF_PARTICLES;
	hr = device->CreateUnorderedAccessView(sortedIndicesBuffer, &uavDesc, &sortedIndicesUAV);

	// Marching Cubes
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeof(SimpleVertex) * ((64 *64*64*5)*3);
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = sizeof(SimpleVertex);
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&desc, nullptr, &vertexBuffer);

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = ((64 * 64 * 64 * 5) * 3);
	hr = device->CreateUnorderedAccessView(vertexBuffer, &uavDesc, &vertexUAV);


	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeof(UINT) * ((64 * 64 * 64 * 5) * 3);
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = sizeof(UINT);
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&desc, nullptr, &indexBuffer);

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = ((64 * 64 * 64 * 5) * 3);
	hr = device->CreateUnorderedAccessView(indexBuffer, &uavDesc, &indexUAV);

	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeof(UINT) * 1;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = sizeof(UINT);
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&desc, nullptr, &vertexCountBuffer);

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R32_UINT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	hr = device->CreateUnorderedAccessView(vertexCountBuffer, &uavDesc, &vertexCountUAV);


	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = sizeof(UINT) * 1;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = sizeof(UINT);
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&desc, nullptr, &indexCountBuffer);

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	hr = device->CreateUnorderedAccessView(indexCountBuffer, &uavDesc, &indexCountUAV);
}

void SPH::UpdateSpatialGridClear(float deltaTime)
{
	// Bind compute shader and buffers (double buffer the grid)
	deviceContext->CSSetShader(SpatialGridClearShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &SpatialGridConstantBuffer);


	deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, &outputUAVSpatialGridCountA, nullptr);
	deviceContext->CSSetUnorderedAccessViews(6, 1, &sortedIndicesUAV, nullptr);


	// Dispatch compute shader
	deviceContext->Dispatch(threadGroupCountX, 1, 1);

	// Unbind UAVs
	deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(2, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(6, 1, uavViewNull, nullptr);

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

void SPH::UpdateRadixSorting(float deltaTime)
{
	const UINT numPasses = 32 / RADIX_BITS;

	// Initialize values (currentBit = 0, passCount = 0)
	UINT initData[4] = { 0, 0, 0, 0 };
	deviceContext->UpdateSubresource(globalParamsBuffer, 0, nullptr, initData, 0, 0);

	for (UINT pass = 0; pass < numPasses; ++pass) 
	{
		// Histogram Pass
		deviceContext->CSSetShader(RadixHistogramShader, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &histogramUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &globalParamsUAV, nullptr);
		deviceContext->Dispatch(threadGroupCountX, 1, 1);
		deviceContext->CSSetUnorderedAccessViews(1, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		// Prefix Sum Pass
		deviceContext->CSSetShader(RadixPreFixSumShader, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &histogramUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(4, 1, &preFixSumUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &globalParamsUAV, nullptr);
		deviceContext->Dispatch(1, 1, 1);
		deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		// Prepare Offsets Pass
		deviceContext->CSSetShader(RadixPrepareOffsetsShader, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(4, 1, &preFixSumUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &globalParamsUAV, nullptr);
		deviceContext->Dispatch(1, 1, 1);
		deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);

		// Scatter Pass
		deviceContext->CSSetShader(RadixScatterShader, nullptr, 0);
		deviceContext->CSSetUnorderedAccessViews(1, 1, &outputUAVSpatialGridA, nullptr);
		deviceContext->CSSetUnorderedAccessViews(3, 1, &histogramUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(4, 1, &preFixSumUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, &globalParamsUAV, nullptr);
		deviceContext->CSSetUnorderedAccessViews(6, 1, &sortedIndicesUAV, nullptr);
		deviceContext->Dispatch(threadGroupCountX, 1, 1);
		deviceContext->CSSetUnorderedAccessViews(3, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(4, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(5, 1, uavViewNull, nullptr);
		deviceContext->CSSetUnorderedAccessViews(6, 1, uavViewNull, nullptr);
		deviceContext->CSSetShader(nullptr, nullptr, 0);
	}
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
			particle->density = outputData[i].density;
		}
		deviceContext->Unmap(outputResultBuffer, 0);
	}
}

void SPH::UpdateMarchingCubes(float deltaTime)
{
	deviceContext->CSSetShader(MarchingCubesShader, nullptr, 0);
	deviceContext->CSSetUnorderedAccessViews(7, 1, &vertexUAV, nullptr);
	deviceContext->CSSetUnorderedAccessViews(8, 1, &indexUAV, nullptr);
	deviceContext->CSSetUnorderedAccessViews(9, 1, &vertexCountUAV, nullptr);
	deviceContext->CSSetUnorderedAccessViews(10, 1, &indexCountUAV, nullptr);
	UINT groupsX = (64 + 7) / 8;
	UINT groupsY = (64 + 7) / 8;
	UINT groupsZ = (64 + 7) / 8;
	deviceContext->Dispatch(groupsX, groupsY, groupsZ);
	deviceContext->CSSetUnorderedAccessViews(7, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(8, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(9, 1, uavViewNull, nullptr);
	deviceContext->CSSetUnorderedAccessViews(10, 1, uavViewNull, nullptr);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void SPH::Update(float deltaTime, float minX, float minZ)
{
	// SPH
	UpdateSpatialGridClear(deltaTime);
	UpdateAddParticlesToSpatialGrid(deltaTime);
	UpdateBitonicSorting(deltaTime);
	//UpdateRadixSorting(deltaTime);
	UpdateBuildGridOffsets(deltaTime);
	UpdateParticleDensities(deltaTime);
	UpdateParticlePressure(deltaTime);
	UpdateIntegrateComputeShader(deltaTime, minX, minZ);

	// Marching Cubes
	//UpdateMarchingCubes(deltaTime);
}