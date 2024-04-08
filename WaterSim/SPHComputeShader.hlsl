
cbuffer ParticleConstantBuffer : register(b1)
{
	int particleCount;
	float wallStiffness;
	float deltaTime;
	float gravity;

	float4 vPlanes[6];
};

struct IntegrateParticle
{
    float3 position;
    float padding01;

    float3 velocity;
    float padding02;
};

StructuredBuffer<IntegrateParticle> IntegrateInput : register(t0); // Input // Shader Resource
RWStructuredBuffer<IntegrateParticle> IntegrateOutput : register(u0); // Output // UAV

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    float3 position = IntegrateInput[threadID].position;
    float3 velocity = IntegrateInput[threadID].velocity;


    
    
    velocity.y +=  -gravity * deltaTime;
    position += velocity * deltaTime;

    IntegrateOutput[threadID].position += velocity * deltaTime;
    IntegrateOutput[threadID].velocity = 0.0f;
}
