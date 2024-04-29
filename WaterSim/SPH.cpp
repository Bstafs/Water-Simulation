#include "SPH.h"

SPH::SPH(int numbParticles, ID3D11DeviceContext* contextdevice, ID3D11Device* device)
{

	deviceContext = contextdevice;
	this->device = device;

	// Particle Resize - Since I'm Going to be looping through the particles 3 times for the x,y,z position.
	int particleResize = NUM_OF_PARTICLES;
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

}

void SPH::InitParticles()
{

	// Particle Initialization
	float spacing = 5.0f; // Adjust spacing as needed
	int particlesPerDimension = powf(NUM_OF_PARTICLES, 1.0 / 3.0); // Calculate particles per dimension

	float offsetX = -spacing * (particlesPerDimension - 1) / 2.0f;
	float offsetY = -spacing * (particlesPerDimension - 1) / 2.0f;
	float offsetZ = -spacing * (particlesPerDimension - 1) / 2.0f;

	for (int i = 0; i < NUM_OF_PARTICLES; i++)
	{
		Particle* newParticle = new Particle(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 1.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), 1.5f, XMFLOAT3(1.0f, 1.0f, 1.0f));

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

	FluidSimComputeShader = CreateComputeShader(L"SPHComputeShader.hlsl", "CSMain", device);
	SetDebugName(FluidSimComputeShader, "Update Positions");

	ParticlePosition* positions = new ParticlePosition[NUM_OF_PARTICLES];

	for (int i = 0; i < NUM_OF_PARTICLES; i++)
	{
		positions[i].positions.x = particleList[i]->position.x;
		positions[i].positions.y = particleList[i]->position.y;
		positions[i].positions.z = particleList[i]->position.z;
	}

	positionBuffer = CreateStructureBuffer(sizeof(ParticlePosition), (float*)positions, NUM_OF_PARTICLES, device);
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

float SPH::SmoothingKernel(float dst, float radius)
{
	if (dst <= radius)
	{
		float scale = 15 / (2 * PI * pow(radius, 5));
		float v = radius - dst;
		return v * v * scale;
	}
	return 0;
}

float SPH::SmoothingKernelDerivative(float radius, float dst)
{
	if (dst <= radius)
	{
		float scale = 15 / (pow(radius, 5) * PI);
		float v = radius - dst;
		return -v * scale;
	}
	return 0;
}

float SPH::CalculateMagnitude(const XMFLOAT3& vector)
{
	XMVECTOR xmVector = XMLoadFloat3(&vector);
	XMVECTOR magnitudeVector = XMVector3Length(xmVector);
	float magnitude;
	XMStoreFloat(&magnitude, magnitudeVector);
	return magnitude;
}

float SPH::CalculateDensity(XMFLOAT3 samplePoint)
{
	float density = 1.0f;
	const float mass = 1.0f;

	for (int i = 0; i < particleList.size(); i++)
	{
		float particlePositionMagnitude = CalculateMagnitude(particleList[i]->position);
		float samplePointMagnitude = CalculateMagnitude(samplePoint);
		float dst = particlePositionMagnitude - samplePointMagnitude;

		float influence = SmoothingKernel(dst, particleList[i]->smoothingRadius);

		density += mass * influence;
	}

	return density;
}

float SPH::ConvertDensityToPressure(float density)
{
	float densityError = density - targetDensity;
	float pressure = densityError * pressureMulti;
	return pressure;
}

float SPH::CalculateSharedPressure(float densityA, float densityB)
{
	float pressureA = ConvertDensityToPressure(densityA);
	float pressureB = ConvertDensityToPressure(densityB);
	return(pressureA + pressureB) / 2.0f;
}

// Function to get a random direction
XMFLOAT3 GetRandomDir()
{
	// Use a random number generator
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<float> dis(-1.0f, 1.0f); // Generate random numbers between -1 and 1

	// Generate random components for x, y, and z
	float x = dis(gen);
	float y = dis(gen);
	float z = dis(gen);

	// Normalize the vector to ensure it's a direction
	float length = sqrt(x * x + y * y + z * z);
	return XMFLOAT3(x / length, y / length, z / length);
}

XMFLOAT3 SPH::CalculatePressureForce(int particleIndex)
{
	XMFLOAT3 pressureForce = XMFLOAT3(1.0f, 1.0f, 1.0f);
	XMFLOAT3 dir = XMFLOAT3(1.0f, 1.0f, 1.0f);
	float slope = 0.0f;

	for (int otherParticleIndex = 0; otherParticleIndex < particleList.size(); otherParticleIndex++)
	{
		if (particleIndex == otherParticleIndex) continue;

		XMFLOAT3 offset;
		offset.x = particleList[otherParticleIndex]->position.x - particleList[particleIndex]->position.x;
		offset.y = particleList[otherParticleIndex]->position.y - particleList[particleIndex]->position.y;
		offset.z = particleList[otherParticleIndex]->position.z - particleList[particleIndex]->position.z;

		float dst = CalculateMagnitude(offset);

		float density = particleList[otherParticleIndex]->density;

		float dirX = offset.x / dst;
		float dirY = offset.y / dst;
		float dirZ = offset.z / dst;
		dir = XMFLOAT3(dirX, dirY, dirZ);
		slope = SmoothingKernelDerivative(particleList[particleIndex]->smoothingRadius, -dst);

		float sharedPressure = CalculateSharedPressure(density, particleList[particleIndex]->density);

		pressureForce.x += -sharedPressure * dir.x * slope / density;
		pressureForce.y += -sharedPressure * dir.y * slope / density;
		pressureForce.z += -sharedPressure * dir.z * slope / density;


		return pressureForce;
	}
}

void SPH::Update(float deltaTime)
{
	// Setup Particle Forces
	//ParticleForcesSetup();

	float dampingFactor = 0.95f;

	float minX = -20.0f;
	float maxX = 20.0f;
	float minY = -10.0f;
	float maxY = 20.0f;
	float minZ = -20.0f;
	float maxZ = 20.0f;

	for (int i = 0; i < particleList.size(); i++)
	{
		particleList[i]->velocity.y += -9.81f * deltaTime;
		particleList[i]->density = CalculateDensity(particleList[i]->position);

		particleList[i]->pressureForce = CalculatePressureForce(i);

		// A = F / M
		particleList[i]->acceleration.x += particleList[i]->pressureForce.x / particleList[i]->density;
		particleList[i]->acceleration.y += particleList[i]->pressureForce.y / particleList[i]->density;
		particleList[i]->acceleration.z += particleList[i]->pressureForce.z / particleList[i]->density;

		particleList[i]->velocity.x += particleList[i]->acceleration.x * deltaTime;
		particleList[i]->velocity.y += particleList[i]->acceleration.y * deltaTime;
		particleList[i]->velocity.x += particleList[i]->acceleration.z * deltaTime;

		//particleList[i]->velocity.x += deltaTime;
		//particleList[i]->velocity.z += deltaTime;

		particleList[i]->position.x += particleList[i]->velocity.x * deltaTime;
		particleList[i]->position.y += particleList[i]->velocity.y * deltaTime;
		particleList[i]->position.z += particleList[i]->velocity.x * deltaTime;

		// Basic Bounding Box
		{
			if (particleList[i]->position.x < minX) {
				particleList[i]->position.x = minX;
				particleList[i]->velocity.x *= -1 * dampingFactor; // Reverse velocity upon collision
			}
			else if (particleList[i]->position.x > maxX) {
				particleList[i]->position.x = maxX;
				particleList[i]->velocity.x *= -1 * dampingFactor;
			}

			if (particleList[i]->position.y < minY) {
				particleList[i]->position.y = minY;
				particleList[i]->velocity.y *= -1 * dampingFactor;
			}
			else if (particleList[i]->position.y > maxY) {
				particleList[i]->position.y = maxY;
				particleList[i]->velocity.y *= -1 * dampingFactor;
			}

			if (particleList[i]->position.z < minZ) {
				particleList[i]->position.z = minZ;
				particleList[i]->velocity.z *= -1 * dampingFactor;
			}
			else if (particleList[i]->position.z > maxZ) {
				particleList[i]->position.z = maxZ;
				particleList[i]->velocity.z *= -1 * dampingFactor;
			}
		}
	}
}
