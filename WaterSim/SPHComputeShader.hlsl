#include "FluidMaths.hlsl"

struct ParticlePosition
{
    float3 position;
    float deltaTime;
    
    float3 velocity;
    float density;
};

StructuredBuffer<ParticlePosition> InputPosition : register(t0);

RWStructuredBuffer<ParticlePosition> OutputPosition : register(u0); // Output // UAV

float targetDensity = 8.0f;
float stiffnessValue = 30.0f;

float ConvertDensityToPressure(float density)
{
    float densityError = density - targetDensity;
    float pressure = densityError * stiffnessValue;
    return pressure;
}

float CalculateSharedPressure(float densityA, float densityB)
{
    float pressureA = ConvertDensityToPressure(densityA);
    float pressureB = ConvertDensityToPressure(densityB);
    return (pressureA + pressureB) / 2.0f;
}

float CalculateDensity(float3 samplePoint)
{
    float density = 1.0f;
    
    return density;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    if (threadID >= 512)
        return;
    
    float3 inputPosition = InputPosition[threadID].position;
    float3 inputVelocity = InputPosition[threadID].velocity;
    float inputDensity = InputPosition[threadID].density;
    float deltaTime = InputPosition[threadID].deltaTime;
   
    inputVelocity.y += -9.81f * deltaTime;
    
    inputDensity = CalculateDensity(inputPosition);

    OutputPosition[threadID].position = inputPosition;
    OutputPosition[threadID].velocity = inputVelocity;
    OutputPosition[threadID].density = inputDensity;
}
