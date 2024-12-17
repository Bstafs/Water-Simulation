#include "SPH.h"

SPH::SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device)
	: spatialGrid(10.0f), // Provide a valid value for `cellSize`
	deviceContext(contextdevice),
	device(device)
{
	deviceContext = contextdevice;
	this->device = device;

	// Particle Resize - Since I'm Going to be looping through the particles 3 times for the x,y,z position.
	int particleResize = NUM_OF_PARTICLES;
	particleList.resize(particleResize);

	// Particle Initialization
	InitRandomDirections();
	InitParticles();
	InitComputeShader();
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

	if (FluidSimComputeShader) FluidSimComputeShader->Release();
	if (inputBuffer) inputBuffer->Release();
	if (inputView) inputView->Release();
	if (outputUAV) outputUAV->Release();
	if (outputBuffer) outputBuffer->Release();

}

void SPH::InitParticles()
{

	// Particle Initialization
	float spacing = 5.0f; // Adjust spacing as needed
	int particlesPerDimension = static_cast<int>(std::cbrt(NUM_OF_PARTICLES));

	float offsetX = -spacing * (particlesPerDimension - 1) / 2.0f;
	float offsetY = -spacing * (particlesPerDimension - 1) / 2.0f;
	float offsetZ = -spacing * (particlesPerDimension - 1) / 2.0f;

	for (int i = 0; i < NUM_OF_PARTICLES; i++)
	{
		Particle* newParticle = new Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 0.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), 2.5f, XMFLOAT3(1.0f, 1.0f, 1.0f));

		int xIndex = i % particlesPerDimension;
		int yIndex = (i / particlesPerDimension) % particlesPerDimension;
		int zIndex = i / (particlesPerDimension * particlesPerDimension);

		float x = offsetX + xIndex * spacing;
		float y = offsetY + yIndex * spacing;
		float z = offsetZ + zIndex * spacing;

		newParticle->position.x = x;
		newParticle->position.y = y;
		newParticle->position.z = z;

		particleList[i] = newParticle;
	}

}

void SPH::InitComputeShader()
{
	HRESULT hr;

	FluidSimComputeShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	SetDebugName(FluidSimComputeShader, "Update Positions");

	ParticlePosition* position = new ParticlePosition[NUM_OF_PARTICLES];

	for (int i = 0; i < particleList.size(); i++)
	{
		Particle* particle = particleList[i];

		position[i].velocity = particle->velocity.y;
	}

	inputBuffer = CreateStructureBuffer(sizeof(ParticlePosition), (float*)position, NUM_OF_PARTICLES, device);

	inputView = CreateShaderResourceView(inputBuffer, NUM_OF_PARTICLES, device);

	D3D11_BUFFER_DESC outputDesc;
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.ByteWidth = sizeof(ParticlePosition) * NUM_OF_PARTICLES;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.StructureByteStride = sizeof(ParticlePosition);
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&outputDesc, 0, &outputBuffer);

	outputDesc.Usage = D3D11_USAGE_STAGING;
	outputDesc.BindFlags = 0;
	outputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = device->CreateBuffer(&outputDesc, 0, &outputResultBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = NUM_OF_PARTICLES;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUAV);
}

// Density Force Kernel.
float SPH::DensitySmoothingKernel(float dst, float radius)
{
	if (dst >= 0 && dst <= radius) {
		const float scale = 15.0f / (2.0f * XM_PI * pow(abs(radius), 5));
		float diff = radius - dst;
		return diff * diff * scale;
	}
	return 0.0f;
}

// Pressure Force Kernel
float SPH::PressureSmoothingKernel(float dst, float radius)
{
	if (dst >= 0 && dst <= radius) {
		const float scale = 15.0f / (pow(abs(radius), 5) * XM_PI);
		float diff = radius - dst;
		return -diff * scale;
	}
	return 0.0f;
}

float SPH::NearDensitySmoothingKernel(float dst, float radius)
{
	if (dst >= 0 && dst <= radius) {
		const float scale = 15.0f / (XM_PI * pow(abs(radius), 6));
		float diff = radius - dst;
		return diff * diff * diff * scale;
	}
	return 0.0f;
}

float SPH::NearDensitySmoothingKernelDerivative(float dst, float radius)
{
	if (dst >= 0 && dst <= radius) {
		const float scale = 45.0f / (pow(abs(radius), 6) * XM_PI);
		float diff = radius - dst;
		return -diff * diff * scale;
	}
	return 0.0f;
}

float SPH::ViscositySmoothingKernel(float dst, float radius)
{
	if (dst >= 0 && dst <= radius) {
		const float scale = 315.0f / (64 * XM_PI * pow(abs(radius), 9));
		float diff = radius * radius - dst * dst;
		return diff * diff * diff * scale;
	}
	return 0.0f;
}

float SPH::CalculateMagnitude(const XMFLOAT3& vector)
{
	XMVECTOR xmVector = XMLoadFloat3(&vector);
	XMVECTOR magnitudeVector = XMVector3Length(xmVector);
	float magnitude;
	XMStoreFloat(&magnitude, magnitudeVector);
	return magnitude;
}

XMFLOAT3 Subtract(const XMFLOAT3& a, const XMFLOAT3& b) {
	return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

float SPH::CalculateDensity(const XMFLOAT3& samplePoint)
{
	float density = 0.0f;
	const float mass = 1.0f; // Assumed mass for particles

	// Get neighboring cells
	std::vector<int> neighbors = spatialGrid.GetNeighboringParticles(samplePoint);

	for (int neighborIndex : neighbors) {
		Particle* neighbor = particleList[neighborIndex];

		// Calculate the distance between the sample point and the neighbor's position
		XMFLOAT3 offset = Subtract(samplePoint, neighbor->position);
		float dst = CalculateMagnitude(offset);

		// Apply the near density kernel
		float influence = DensitySmoothingKernel(dst, neighbor->smoothingRadius);

		// Increment the near density
		density += mass * influence;
	}

	return density;
}

float SPH::CalculateNearDensity(const XMFLOAT3& samplePoint)
{
	float nearDensity = 0.0f;
	const float mass = 1.0f; // Assumed mass for particles

	// Get neighboring cells
	std::vector<int> neighbors = spatialGrid.GetNeighboringParticles(samplePoint);

	for (int neighborIndex : neighbors) {
		Particle* neighbor = particleList[neighborIndex];

		// Calculate the distance between the sample point and the neighbor's position
		XMFLOAT3 offset = Subtract(samplePoint, neighbor->position);
		float dst = CalculateMagnitude(offset);

		// Apply the near density kernel
		float influence = NearDensitySmoothingKernel(dst, neighbor->smoothingRadius);

		// Increment the near density
		nearDensity += mass * influence;
	}

	return nearDensity;
}

float SPH::ConvertDensityToPressure(float density)
{
	float densityError = density - targetDensity;
	float pressure = densityError * stiffnessValue;
	return pressure;
}

float SPH::CalculateSharedPressure(float densityA, float densityB)
{
	float pressureA = ConvertDensityToPressure(densityA);
	float pressureB = ConvertDensityToPressure(densityB);
	return(pressureA + pressureB) / 2.0f;
}

// Initialize random directions once
void SPH::InitRandomDirections() {
	randomDirections.resize(numRandomDirections);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

	for (int i = 0; i < numRandomDirections; ++i) {
		float x = dis(gen);
		float y = dis(gen);
		float z = dis(gen);
		float length = sqrt(x * x + y * y + z * z);
		randomDirections[i] = XMFLOAT3(x / length, y / length, z / length);
	}
}

// Function to get a random direction
XMFLOAT3 SPH::GetRandomDir()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dis(0, numRandomDirections - 1);
	return randomDirections[dis(gen)];
}

XMFLOAT3 SPH::CalculatePressureForceWithRepulsion(int particleIndex) {
	XMFLOAT3 pressureForce = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 repulsionForce = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 viscousForce = XMFLOAT3(0.0f, 0.0f, 0.0f);

	Particle* currentParticle = particleList[particleIndex];
	std::vector<int> neighbors = spatialGrid.GetNeighboringParticles(currentParticle->position);

	float viscosityCoefficient = 0.05f; // Adjust viscosity as needed

	for (int neighborIndex : neighbors) {
		Particle* neighbor = particleList[neighborIndex];
		if (neighbor == currentParticle) continue;

		// Relative velocity
		XMFLOAT3 relativeVelocity = Subtract(currentParticle->velocity, neighbor->velocity);

		// Calculate offset and distance
		XMFLOAT3 offset = Subtract(neighbor->position, currentParticle->position);
		float distance = CalculateMagnitude(offset);

		// Skip particles too close or too far
		if (distance < 1e-6f || distance > currentParticle->smoothingRadius) continue;

		// Normalize direction
		XMFLOAT3 direction = XMFLOAT3(offset.x / distance, offset.y / distance, offset.z / distance);

		// Smoothing kernel derivatives
		float poly6 = ViscositySmoothingKernel(distance, currentParticle->smoothingRadius);
		float kernelDerivative = PressureSmoothingKernel(distance, currentParticle->smoothingRadius);
		float nearKernelDerivative = NearDensitySmoothingKernelDerivative(distance, currentParticle->smoothingRadius);

		// Calculate shared pressure
		float sharedPressure = CalculateSharedPressure(currentParticle->density, neighbor->density);

		// Calculate shared near pressure
		float sharedNearPressure = CalculateSharedPressure(currentParticle->nearDensity, neighbor->nearDensity);

		// Pressure force
		pressureForce.x += direction.x * kernelDerivative * -sharedPressure;
		pressureForce.y += direction.y * kernelDerivative * -sharedPressure;
		pressureForce.z += direction.z * kernelDerivative * -sharedPressure;

		// Near-pressure force
		repulsionForce.x += direction.x * nearKernelDerivative * -sharedNearPressure;
		repulsionForce.y += direction.y * nearKernelDerivative * -sharedNearPressure;
		repulsionForce.y += direction.z * nearKernelDerivative * -sharedNearPressure;

		// Viscous force
		viscousForce.x += viscosityCoefficient * relativeVelocity.x * poly6;
		viscousForce.y += viscosityCoefficient * relativeVelocity.y * poly6;
		viscousForce.z += viscosityCoefficient * relativeVelocity.z * poly6;
	}

	// Combine forces
	XMFLOAT3 totalForce;
	totalForce.x = pressureForce.x + repulsionForce.x + viscousForce.x;
	totalForce.y = pressureForce.y + repulsionForce.y + viscousForce.y;
	totalForce.z = pressureForce.z + repulsionForce.z + viscousForce.z;

	return totalForce;
}

void SPH::UpdateSpatialGrid()
{
	spatialGrid.Clear();

	for (int i = 0; i < particleList.size(); ++i)
	{
		spatialGrid.AddParticle(i, particleList[i]->position);
	}
}

void SPH::UpdateComputeShader()
{
	// Tell the Compute Shader most recent values
	D3D11_MAPPED_SUBRESOURCE mappedInputResource;
	HRESULT hr = deviceContext->Map(inputBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedInputResource);
	if (SUCCEEDED(hr)) 
	{
		ParticlePosition* inputData = reinterpret_cast<ParticlePosition*>(mappedInputResource.pData);
		for (int i = 0; i < particleList.size(); ++i) 
		{
			inputData[i].velocity = particleList[i]->velocity.y;
		}
		deviceContext->Unmap(inputBuffer, 0);
	}

	// Bind compute shader and buffers
	deviceContext->CSSetShader(FluidSimComputeShader, nullptr, 0);
	deviceContext->CSSetShaderResources(0, 1, &inputView);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &outputUAV, nullptr);

	// Dispatch compute shader
	deviceContext->Dispatch((NUM_OF_PARTICLES + 255) / 256, 1, 1);

	// Unbind resources
	ID3D11ShaderResourceView* nullSRV[] = { nullptr };
	ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
	deviceContext->CSSetShaderResources(0, 1, nullSRV);
	deviceContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

	deviceContext->CSSetShader(nullptr, nullptr, 0);

	deviceContext->CopyResource(outputResultBuffer, outputBuffer);

	// Map output to a new buffer and apply new values to particles
	D3D11_MAPPED_SUBRESOURCE mappedOutputResource;
	hr = deviceContext->Map(outputResultBuffer, 0, D3D11_MAP_READ, 0, &mappedOutputResource);
	if (SUCCEEDED(hr)) 
	{
		ParticlePosition* outputData = reinterpret_cast<ParticlePosition*>(mappedOutputResource.pData);
		for (int i = 0; i < particleList.size(); ++i) 
		{
			particleList[i]->velocity.y = outputData[i].velocity;
		}
		deviceContext->Unmap(outputResultBuffer, 0);
	}
}

void SPH::Update(float deltaTime, float minX, float maxX)
{
	UpdateComputeShader();
	UpdateSpatialGrid();

	for (int i = 0; i < particleList.size(); ++i) 
	{
		Particle* particle = particleList[i];

		// Apply forces and update properties
		//particle->velocity.y += -9.81f * deltaTime;
		particle->density = CalculateDensity(particle->position);
		particle->nearDensity = CalculateNearDensity(particle->position);
		particle->pressureForce = CalculatePressureForceWithRepulsion(i);

		// Acceleration = Force / Density
		particle->acceleration.x = particle->pressureForce.x / particle->density;
		particle->acceleration.y = particle->pressureForce.y / particle->density;
		particle->acceleration.z = particle->pressureForce.z / particle->density;

		// Update velocity and position
		particle->velocity.x += particle->acceleration.x * deltaTime;
		particle->velocity.y += particle->acceleration.y * deltaTime;
		particle->velocity.z += particle->acceleration.z * deltaTime;

		particle->position.x += particle->velocity.x * deltaTime;
		particle->position.y += particle->velocity.y * deltaTime;
		particle->position.z += particle->velocity.z * deltaTime;

		// Handle boundary collisions with damping (as in the existing implementation)
		if (particle->position.x < minX) {
			particle->position.x = minX;
			particle->velocity.x *= -1; // Reverse velocity
		}
		else if (particle->position.x > maxX) {
			particle->position.x = maxX;
			particle->velocity.x *= -1;
		}

		if (particle->position.y < minY) {
			particle->position.y = minY;
			particle->velocity.y *= -1;
		}
		else if (particle->position.y > maxY) {
			particle->position.y = maxY;
			particle->velocity.y *= -1;
		}

		if (particle->position.z < minZ) {
			particle->position.z = minZ;
			particle->velocity.z *= -1;
		}
		else if (particle->position.z > maxZ) {
			particle->position.z = maxZ;
			particle->velocity.z *= -1;
		}

		// Apply damping to velocity after collision
		particle->velocity.x *= dampingFactor;
		particle->velocity.y *= dampingFactor;
		particle->velocity.z *= dampingFactor;
	}

}