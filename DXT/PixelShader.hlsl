#include "ShaderTypes.hlsli"

float4 main(VertexShaderOutput input) : SV_TARGET
{
	float3 lightDirection = normalize(float3(2.0f, 4.0f, 1.0f));

	float light = saturate(dot(lightDirection, input.Normal));
	float ambient = 0.2f;
	float result = saturate(ambient + light);

	return float4(result, result, result, 1.0f);
}