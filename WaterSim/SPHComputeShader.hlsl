struct ConstantParticleData
{
    float3 position;
    float3 velocity;
};

struct ParticleData
{
    float3 position;
    float3 velocity;
};

StructuredBuffer<ConstantParticleData> inputConstantParticleData : register(t0);
RWStructuredBuffer<ParticleData> outputParticleData : register(u0);

[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float3 position = inputConstantParticleData[dispatchThreadID.x].position;
    float3 velocity = inputConstantParticleData[dispatchThreadID.x].velocity;

    position.x += 1000.0f;
    velocity += 100.0f;

    outputParticleData[dispatchThreadID.x].position = position;
    outputParticleData[dispatchThreadID.x].velocity = velocity;
}