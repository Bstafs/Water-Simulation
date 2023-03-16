//--------------------------------------------------------------------------------------
// Water Shader using the Fresnel Effect Factor
//--------------------------------------------------------------------------------------
#define DRAG_MULT 0.048

Texture2D reflectionTexture : register(t0);
Texture2D refractionTexture : register(t1);
Texture2D skyboxTexture : register(t2);

SamplerState samLinear : register(s0);

struct Light
{
    float4 AmbientLight;
    float4 DiffuseLight;
    float4 SpecularLight;

    float SpecularPower;
    float3 LightVecW;
};

cbuffer WaterBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    

    float4 waterColor;
    float4 padding00;
    float4 reflectionTint;
    float4 refractionTint;

    float2 waterSpeed;
    float refractionAmount;
    float fresnelPower;
    float4 specularColor;
    float4 skyBoxColor;
    float4 padding01;

    Light light;
}

struct VS_INPUT
{
    float4 pos : POSITION;
    float2 tex : TEXCOORD;
};

//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    output.pos = mul(input.pos, World);
    output.pos = mul(output.pos, View);
    output.pos = mul(output.pos, Projection);
    output.tex = input.tex;

    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_Target
{

    return float4(0,0,0.6,1);
}