//--------------------------------------------------------------------------------------
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// the lighting equations in this code have been taken from https://www.3dgep.com/texturing-lighting-directx-11/
// with some modifications by David White

Texture2D txNormal : register(t0);
Texture2D txDiffuse : register(t1);
Texture2D txSpecular : register(t2);
Texture2D txPosition : register(t3);
Texture2D txAmbient : register(t4);
Texture2D txEmissive : register(t5);

Texture2D txShadow : register(t6);

SamplerState cmpSampler : register(s0);

#define MAX_LIGHTS 1
// Light types.
#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light
{
    float4 Position; // 16 bytes
	//----------------------------------- (16 byte boundary)
    float4 Direction; // 16 bytes
	//----------------------------------- (16 byte boundary)
    float4 Color; // 16 bytes
	//----------------------------------- (16 byte boundary)
    float SpotAngle; // 4 bytes
    float ConstantAttenuation; // 4 bytes
    float LinearAttenuation; // 4 bytes
    float QuadraticAttenuation; // 4 bytes
	//----------------------------------- (16 byte boundary)
    int LightType; // 4 bytes
    bool Enabled; // 4 bytes
    int LinearDepth;
    int padding09;
	//----------------------------------- (16 byte boundary)
}; // Total:                           // 80 bytes (5 * 16)

cbuffer LightProperties : register(b2)
{
    float4 EyePosition; // 16 bytes
	//----------------------------------- (16 byte boundary)
    float4 GlobalAmbient; // 16 bytes
	//----------------------------------- (16 byte boundary)
    Light Lights[MAX_LIGHTS]; // 80 * 8 = 640 bytes
    
    matrix lightSpaceMatrix;
};

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float4 LSM : lightSpaceMatrix;
};


void GetGBufferAttributes(in float2 screenPos, out float3 normal, out float3 diffuse, out float3 specular, out float3 position, out float3 ambient, out float3 emissive, out float specularPower)
{
    int3 sampleIndices = int3(screenPos.xy, 0);
    normal = txNormal.Load(sampleIndices).xyz;
    diffuse = txDiffuse.Load(sampleIndices).xyz;
    float4 spec = txSpecular.Load(sampleIndices);
    specular = spec.xyz;
    specularPower = spec.w;
    position = txPosition.Load(sampleIndices).xyz;
    ambient = txAmbient.Load(sampleIndices).xyz;
    emissive = txEmissive.Load(sampleIndices).xyz;
}

float4 DoSpecular(Light lightObject, float3 vertexToEye, float3 lightDirectionToVertex, float3 Normal, float specPow)
{
    float3 R = normalize(reflect(-lightDirectionToVertex, Normal));
    float RdotV = max(0, dot(R, vertexToEye));
    
    float H = normalize(lightDirectionToVertex + vertexToEye);
    float NdotH = max(0, dot(NdotH, H));
    
    return lightObject.Color * pow(RdotV, specPow);
}

float DoAttenuation(Light light, float d)
{
    return 1.0f / (light.ConstantAttenuation + light.LinearAttenuation * d + light.QuadraticAttenuation * d * d);
}

float3 DoDiffuse(float3 normal, float3 vertexToLight)
{
    float lightAmount = saturate(dot(normal, vertexToLight));
    float3 color = Lights[0].Color * lightAmount;
    return color;
}

void CreateLightPositions(out float3 vertexToEye, out float3 vertexToLight, out float attenuation, out float3 LightDirectionToVertex, in float3 position)
{
    vertexToEye = EyePosition.xyz - position;
    vertexToLight = Lights[0].Position.xyz - position;
    
    vertexToEye = normalize(vertexToEye);
    
    LightDirectionToVertex = -vertexToLight;
    float distance = length(LightDirectionToVertex);
    LightDirectionToVertex = LightDirectionToVertex / distance;
    
    distance = length(vertexToLight);
    vertexToLight = vertexToLight / distance;
        
    // Attenuation
    attenuation = DoAttenuation(Lights[0], distance);

}

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * 0.01f * 100.0f) / (100.0f + 0.01f - z * (100.0f - 0.01f));
    
    // 0.01f = near plane clip
    // 100.0f = near plane clip
    // Lazy hardcode 
}

float CalculateShadow(float4 lightSpace, float3 normal, float3 lightDir)
{
    float3 projCoords = lightSpace.xyz / lightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    float depthValue = txShadow.Sample(cmpSampler, projCoords.xy).r;
    
    float closestDepth = 0;
    
    int linearDepthType = Lights[0].LinearDepth;
    
    if (linearDepthType == 0)
    {
        closestDepth = LinearizeDepth(depthValue) / 100.0f;
    }
    else if (linearDepthType == 1)
    {
        closestDepth = depthValue;
    }

    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = currentDepth  - bias > closestDepth ? 1.0 : 0.0;
    
    if (projCoords.z > 1.0)
        shadow = 0.0;
    
    return shadow;
}

float DoSpotCone(Light light, float3 vertexToLight)
{
    float minCos = cos(light.SpotAngle);
    
    float maxCos = (minCos + 1.0f) / 2.0f;
    
    float cosAngle = dot(-light.Direction.xyz, vertexToLight);
    
    return smoothstep(minCos, maxCos, cosAngle);
}

float4 DoDirecitonalLight(in float3 vertexToEye, in float3 vertexToLight, in float3 normal, in float specularPower, in float attenuation, in float3 diffuse, in float3 ambient, in float3 emissive, in float3 LightDirectionToVertex, in float3 position, in float4 LSM)
{
    float3 lightDirection = -Lights[0].Direction.xyz;
    
    lightDirection = normalize(lightDirection);
   
    float shadows = CalculateShadow(LSM, normal, lightDirection);

    float3 finalDiffuse = DoDiffuse(normal, lightDirection);
    float4 spec = DoSpecular(Lights[0], vertexToEye, lightDirection, normal, 32.0f);
    float3 finalSpecular = spec.xyz;
    float3 finalAmbient = (ambient * GlobalAmbient.xyz);
    
    float4 finalColor = float4(emissive + finalAmbient + (finalDiffuse + finalSpecular), 1.0f) * float4(diffuse, 1.0f) * (1.0f - shadows);
  
    return finalColor;
}

float4 DoPointLight(in float3 vertexToEye, in float3 vertexToLight, in float3 normal, in float specularPower, in float attenuation, in float3 diffuse, in float3 ambient, in float3 emissive, in float3 LightDirectionToVertex, in float3 position)
{
    float distance = length(vertexToLight);
    vertexToLight = vertexToLight / distance;
    
    float3 finalDiffuse = DoDiffuse(normal, vertexToLight);
    float4 spec = DoSpecular(Lights[0], vertexToEye, LightDirectionToVertex, normal, 32.0f);
    float3 finalSpecular = spec.xyz;
    float3 finalAmbient = (ambient * GlobalAmbient.xyz);
    
    float4 finalColor = float4(emissive + finalAmbient + (finalDiffuse + finalSpecular), 1.0f) * float4(diffuse, 1.0f);
    
    return finalColor;
}

float4 DoSpotLight(in float3 vertexToEye, in float3 vertexToLight, in float3 normal, in float specularPower, in float attenuation, in float3 diffuse, in float3 ambient, in float3 emissive, in float3 LightDirectionToVertex, in float3 position)
{
    float spotIntensity = DoSpotCone(Lights[0], vertexToLight);
    
    float3 finalDiffuse = DoDiffuse(normal, vertexToLight) * attenuation * spotIntensity;
    float4 spec = DoSpecular(Lights[0], vertexToEye, vertexToLight, normal, 32.0f) * attenuation * spotIntensity;
    float3 finalSpecular = spec.xyz;
    float3 finalAmbient = (ambient * GlobalAmbient.xyz);
    
    float4 finalColor = float4(emissive + finalAmbient + (finalDiffuse + finalSpecular), 1.0f) * float4(diffuse, 1.0f);
    
    return finalColor;
}
//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = input.Pos;
    
    output.Tex = input.Tex;
    return output;
}
//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    float3 normal;
    float3 position;
    float3 diffuse;
    float3 specular;
    float3 ambient;
    float3 emissive;
    float specularPower;
    
    float3 vertexToEye;
    float3 vertexToLight;
    float3 LightDirectionToVertex;
    float attenuation;
    
    GetGBufferAttributes(input.Pos.xy, normal, diffuse, specular, position, ambient, emissive, specularPower);
     
    float4 LSM = mul(float4(position, 1.0f), lightSpaceMatrix);

    CreateLightPositions(vertexToEye, vertexToLight, attenuation, LightDirectionToVertex, position);
    
    float4 finalColor;
    
    int lightNumber = Lights[0].LightType;
    
    switch (lightNumber)
    {
        case 0:
        {
                finalColor = DoDirecitonalLight(vertexToEye, vertexToLight, normal, specularPower, attenuation, diffuse, ambient, emissive, LightDirectionToVertex, position, LSM);
                break;
            }
        case 1:
        {
                finalColor = DoPointLight(vertexToEye, vertexToLight, normal, specularPower, attenuation, diffuse, ambient, emissive, LightDirectionToVertex, position);
                break;
            }
        case 2:
        {
                finalColor = DoSpotLight(vertexToEye, vertexToLight, normal, specularPower, attenuation, diffuse, ambient, emissive, LightDirectionToVertex, position);
                break;
            }
    }
    
    return finalColor;
}