#include "ShaderTypes.hlsli"

cbuffer TransformConstants : register(b0)
{
	float4x4 World;
	float4x4 ViewProjection;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	output.Position = mul(ViewProjection, mul(World, float4(input.pos, 1.0f)));
	output.Normal = mul(World, float4(input.normal, 0.0f)).xyz;
	output.UV = input.uv;
	return output;
}