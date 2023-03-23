//--------------------------------------------------------------------------------------
// Water Shader using the Fresnel Effect Factor
//--------------------------------------------------------------------------------------
#define DRAG_MULT 0.048

Texture2D reflectionTexture : register(t0);
Texture2D refractionTexture : register(t1);
Texture2D skyboxTexture : register(t2);

SamplerState samLinear : register(s0);

cbuffer WaterBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VS_INPUT
{
    float4 pos : POSITION;
    float3 Norm : NORMAL;
    float2 tex : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 Norm : NORMAL;

    float3 posW : POSITION;
    float2 tex : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    float4 wrldPos = mul(input.pos, World);
    output.posW = wrldPos.xyz;

    output.pos = mul(wrldPos, View);
    output.pos = mul(output.pos, Projection);
    output.tex = input.tex;

    float3 normalW = mul(float4(input.Norm, 0.0f), World).xyz;
    output.Norm = normalize(normalW);

    return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_Target
{
    float4 final = skyboxTexture.Sample(samLinear, input.tex);

    return final;
}