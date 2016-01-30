struct VertexShaderInput
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

struct VertexShaderOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
	float3 Normal : NORMAL;
};

struct BlitVertexShaderInput
{
	float2 pos : POSITION;
	float2 uv : TEXCOORD;
};

struct BlitVertexShaderOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};