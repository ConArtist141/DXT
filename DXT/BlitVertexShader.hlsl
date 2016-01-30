#include "ShaderTypes.hlsli"

BlitVertexShaderOutput main(BlitVertexShaderInput input)
{
	BlitVertexShaderOutput output;
	output.Position = float4(input.pos.x, input.pos.y, 0.0f, 1.0f);
	output.UV = input.uv;
	return output;
}