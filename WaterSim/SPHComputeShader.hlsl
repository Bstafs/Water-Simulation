struct ParticlePosition
{
    float gravity;
};

StructuredBuffer<ParticlePosition> InputPosition : register(t0);

RWStructuredBuffer<ParticlePosition> OutputPosition : register(u0); // Output // UAV

[numthreads(128, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    float input = InputPosition[threadID].gravity;
   
    input += -9.81f * 0.016f;

    OutputPosition[threadID].gravity = input;
}
