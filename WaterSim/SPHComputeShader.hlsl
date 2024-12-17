#include "FluidMaths.hlsl"

struct ParticlePosition
{
    float gravity;
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

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    float input = InputPosition[threadID].gravity;
   
    input += -9.81f * 0.016f;

    OutputPosition[threadID].gravity = input;
}
