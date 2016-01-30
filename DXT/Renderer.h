#pragma once

#include "DirectXToolbox.h"

#include <vector>

#define STATIC_MESH_VERTEX_SHADER "VertexShader.cso"
#define STATIC_MESH_PIXEL_SHADER "PixelShader.cso"
#define BLIT_MESH_VERTEX_SHADER "BlitVertexShader.cso"
#define BLIT_MESH_PIXEL_SHADER "BlitPixelShader.cso"

struct StaticMesh
{
	ID3D11Buffer* VertexBuffer;
	ID3D11Buffer* IndexBuffer;
	UINT VertexBufferOffset;
	UINT IndexBufferOffset;
	UINT IndexCount;
};

struct StaticMeshNode
{
	StaticMesh* Mesh;
	DirectX::XMFLOAT3 Position;
	DirectX::XMVECTOR RotationQuaternion;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMMATRIX Transformation;
};

class Scene
{
public:
	std::vector<StaticMeshNode> Meshes;
};

class Renderer
{
public:
	HRESULT Initialize(const DXTRenderParams& params, DXTWindow* window);
	void Render(Scene* scene, DXTCameraBase* camera);
	void Release();

private:
	DXTRenderParams parameters;
	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* context;

	ID3D11Texture2D* diffuseBuffer;
	ID3D11RenderTargetView* diffuseRenderTarget;
	ID3D11ShaderResourceView* diffuseResourceView;
	ID3D11Texture2D* depthTexture;
	ID3D11DepthStencilView* depthStencilView;
	ID3D11RenderTargetView* backBufferRenderTarget;
	ID3D11RasterizerState* rasterizerState;
	ID3D11DepthStencilState* depthStencilStateEnabled;
	ID3D11DepthStencilState* depthStencilStateDisabled;
	ID3D11SamplerState* blitSamplerState;
	ID3D11SamplerState* staticMeshSamplerState;

	ID3D11VertexShader* staticMeshVertexShader;
	ID3D11PixelShader* staticMeshPixelShader;
	ID3D11VertexShader* blitVertexShader;
	ID3D11PixelShader* blitPixelShader;

	ID3D11Buffer* blitVertexBuffer;
	ID3D11InputLayout* blitInputLayout;
	ID3D11InputLayout* staticMeshInputLayout;

	ID3D11Buffer* transformConstantBuffer;
};