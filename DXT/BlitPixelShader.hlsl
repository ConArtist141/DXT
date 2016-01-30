#include "ShaderTypes.hlsli"

texture2D DiffuseTexture : register(t0);

SamplerState Sampler : register(s0);

float4 main(BlitVertexShaderOutput input) : SV_TARGET
{
	return DiffuseTexture.Sample(Sampler, input.UV);
}