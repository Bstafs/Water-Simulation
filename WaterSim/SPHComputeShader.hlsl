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
    float3 tempPos = inputConstantParticleData[dispatchThreadID.x].position;
    float3 tempVel = inputConstantParticleData[dispatchThreadID.x].velocity;

    tempPos.x += 100.0f;
    tempVel.y += 1000.0f;

    outputParticleData[dispatchThreadID.x].position = tempPos;
    outputParticleData[dispatchThreadID.x].velocity = tempVel;
}