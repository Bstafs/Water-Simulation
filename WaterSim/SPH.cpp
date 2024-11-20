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
		Particle* newParticle = new Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 1.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), 1.1f, XMFLOAT3(1.0f, 1.0f, 1.0f));

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

	/*FluidSimComputeShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	SetDebugName(FluidSimComputeShader, "Update Positions");

	ParticlePosition* positions = new ParticlePosition[NUM_OF_PARTICLES];

	for (int i = 0; i < NUM_OF_PARTICLES; i++)
	{
		positions[i].positions.x = particleList[i]->position.x;
		positions[i].positions.y = particleList[i]->position.y;
		positions[i].positions.z = particleList[i]->position.z;
	}

	positionBuffer = CreateStructureBuffer(sizeof(ParticlePosition), (float*)positions, NUM_OF_PARTICLES, device);*/
}

void SPH::SetUpParticleConstantBuffer()
{

}

void SPH::ParticleForcesSetup()
{
	deviceContext->CSSetShader(FluidSimComputeShader, nullptr, 0);

	deviceContext->CSSetShaderResources(0, 1, &pIntegrateSRVOne);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &pIntegrateUAVOne, nullptr);

	deviceContext->Dispatch(NUM_OF_PARTICLES, 1, 1);

	deviceContext->CSSetShaderResources(0, 1, srvNull);
	deviceContext->CSSetUnorderedAccessViews(0, 1, uavViewNull, nullptr);
}

float SPH::SmoothingKernel(float dst, float radius) {
	if (dst >= 0 && dst <= radius) {
		float scale = 315.0f / (64.0f * PI * pow(radius, 9));
		float diff = radius * radius - dst * dst;
		return scale * diff * diff * diff;
	}
	return 0.0f;
}

float SPH::SmoothingKernelDerivative(float dst, float radius) {
	if (dst > 0 && dst <= radius) { // dst > 0 to avoid division by zero
		float scale = -945.0f / (32.0f * PI * pow(radius, 9));
		float diff = radius * radius - dst * dst;
		return scale * diff * diff * dst;
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

float SPH::CalculateDensity(const XMFLOAT3& samplePoint) {
	float density = 1.0f; // Starting density
	const float mass = 1.0f; // Assumed mass for particles

	// Get the neighboring cells
	std::vector<int> neighbors = spatialGrid.GetNeighboringParticles(samplePoint);

	// Iterate over each neighbor particle
	for (int neighborIndex : neighbors) {
		Particle* neighbor = particleList[neighborIndex];

		// Calculate the distance between the sample point and the neighbor's position
		XMFLOAT3 offset = Subtract(samplePoint, neighbor->position);
		float dst = CalculateMagnitude(offset); // CalculateMagnitude already implemented

		// Apply the smoothing kernel to determine influence
		float influence = SmoothingKernel(dst, neighbor->smoothingRadius);

		// Increment the density by the mass * influence
		density += mass * influence;
	}

	return density;
}

float SPH::ConvertDensityToPressure(float density)
{
	return stiffnessValue * (density - targetDensity);
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

XMFLOAT3 SPH::CalculatePressureForce(int particleIndex) {
	XMFLOAT3 pressureForce = XMFLOAT3(0.0f, 0.0f, 0.0f);
	Particle* currentParticle = particleList[particleIndex];

	// Get neighboring particles
	std::vector<int> neighbors = spatialGrid.GetNeighboringParticles(currentParticle->position);

	for (int neighborIndex : neighbors) {
		Particle* neighbor = particleList[neighborIndex];
		if (neighbor == currentParticle) continue;

		// Calculate offset
		XMFLOAT3 offset = Subtract(neighbor->position, currentParticle->position);
		float dst = CalculateMagnitude(offset);

		// Normalize direction
		XMFLOAT3 dir = XMFLOAT3(offset.x / dst, offset.y / dst, offset.z / dst);

		// Calculate smoothing kernel derivative
		float slope = SmoothingKernelDerivative(currentParticle->smoothingRadius, dst);

		// Calculate shared pressure
		float sharedPressure = CalculateSharedPressure(currentParticle->density, neighbor->density);

		// Accumulate pressure force
		pressureForce.x += -sharedPressure * dir.x * slope / currentParticle->density;
		pressureForce.y += -sharedPressure * dir.y * slope / currentParticle->density;
		pressureForce.z += -sharedPressure * dir.z * slope / currentParticle->density;
	}

	return pressureForce;
}

void SPH::UpdateSpatialGrid() 
{
	spatialGrid.Clear();

	for (int i = 0; i < particleList.size(); ++i) 
	{
		spatialGrid.AddParticle(i, particleList[i]->position);
	}
}

void SPH::Update(float deltaTime) {
	UpdateSpatialGrid();

	float dampingFactor = 0.99;

	float minX = -20.0f, maxX = 20.0f;
	float minY = -10.0f, maxY = 20.0f;
	float minZ = -20.0f, maxZ = 20.0f;

	for (int i = 0; i < particleList.size(); ++i) {
		// Apply forces and update velocity
		particleList[i]->velocity.y += -9.81f * deltaTime; // Gravity
		particleList[i]->density = CalculateDensity(particleList[i]->position);
		particleList[i]->pressureForce = CalculatePressureForce(i);

		// Acceleration = Force / Density
		particleList[i]->acceleration.x = particleList[i]->pressureForce.x / particleList[i]->density;
		particleList[i]->acceleration.y = particleList[i]->pressureForce.y / particleList[i]->density;
		particleList[i]->acceleration.z = particleList[i]->pressureForce.z / particleList[i]->density;

		// Update velocity
		particleList[i]->velocity.x += particleList[i]->acceleration.x * deltaTime;
		particleList[i]->velocity.y += particleList[i]->acceleration.y * deltaTime;
		particleList[i]->velocity.z += particleList[i]->acceleration.z * deltaTime;

		// Update position
		particleList[i]->position.x += particleList[i]->velocity.x * deltaTime;
		particleList[i]->position.y += particleList[i]->velocity.y * deltaTime;
		particleList[i]->position.z += particleList[i]->velocity.z * deltaTime;

		// Basic Bounding Box with damping applied after updates
		if (particleList[i]->position.x < minX) {
			particleList[i]->position.x = minX;
			particleList[i]->velocity.x *= -1; // Reverse velocity
		}
		else if (particleList[i]->position.x > maxX) {
			particleList[i]->position.x = maxX;
			particleList[i]->velocity.x *= -1;
		}

		if (particleList[i]->position.y < minY) {
			particleList[i]->position.y = minY;
			particleList[i]->velocity.y *= -1;
		}
		else if (particleList[i]->position.y > maxY) {
			particleList[i]->position.y = maxY;
			particleList[i]->velocity.y *= -1;
		}

		if (particleList[i]->position.z < minZ) {
			particleList[i]->position.z = minZ;
			particleList[i]->velocity.z *= -1;
		}
		else if (particleList[i]->position.z > maxZ) {
			particleList[i]->position.z = maxZ;
			particleList[i]->velocity.z *= -1;
		}

		// Apply damping to velocity after collision
		particleList[i]->velocity.x *= dampingFactor;
		particleList[i]->velocity.y *= dampingFactor;
		particleList[i]->velocity.z *= dampingFactor;
	}
}