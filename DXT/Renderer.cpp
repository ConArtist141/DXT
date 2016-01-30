#include "Renderer.h"

using namespace DirectX;

HRESULT Renderer::Initialize(const DXTRenderParams & params, DXTWindow * window)
{
	parameters = params;

	HRESULT result = DXTInitDevice(params, window, &swapChain, &device, &context);
	if (FAILED(result))
		return result;

	// Create objects
	DXTCreateRenderTargetFromBackBuffer(swapChain, device, &backBufferRenderTarget);
	DXTCreateRenderTarget(device, params.Extent.Width, params.Extent.Height, DXGI_FORMAT_R8G8B8A8_UNORM,
		&diffuseBuffer, &diffuseRenderTarget, &diffuseResourceView);
	DXTCreateDepthStencilBuffer(device, params.Extent.Width, params.Extent.Height, DXGI_FORMAT_D24_UNORM_S8_UINT,
		&depthTexture, &depthStencilView);
	DXTCreateRasterizerStateSolid(device, &rasterizerState);
	DXTCreateDepthStencilStateDepthTestEnabled(device, &depthStencilStateEnabled);
	DXTCreateDepthStencilStateDepthTestDisabled(device, &depthStencilStateDisabled);
	DXTCreateBlitVertexBuffer(device, &blitVertexBuffer);
	DXTCreateSamplerStatePointClamp(device, &blitSamplerState);
	DXTCreateSamplerStateLinearClamp(device, &staticMeshSamplerState);

	// Load shaders
	DXTBytecodeBlob blitBytecodeBlob;
	DXTBytecodeBlob staticMeshBytecodeBlob;
	DXTVertexShaderFromFile(device, BLIT_MESH_VERTEX_SHADER, &blitVertexShader, &blitBytecodeBlob);
	DXTVertexShaderFromFile(device, STATIC_MESH_VERTEX_SHADER, &staticMeshVertexShader, &staticMeshBytecodeBlob);
	DXTPixelShaderFromFile(device, BLIT_MESH_PIXEL_SHADER, &blitPixelShader);
	DXTPixelShaderFromFile(device, STATIC_MESH_PIXEL_SHADER, &staticMeshPixelShader);

	// Create input layouts
	D3D11_INPUT_ELEMENT_DESC staticMeshInputDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 5, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT staticMeshElementCount = 3;

	DXTCreateBlitInputLayout(device, &blitBytecodeBlob, &blitInputLayout);
	device->CreateInputLayout(staticMeshInputDesc, staticMeshElementCount, 
		staticMeshBytecodeBlob.Bytecode, staticMeshBytecodeBlob.BytecodeLength, &staticMeshInputLayout);

	blitBytecodeBlob.Destroy();
	staticMeshBytecodeBlob.Destroy();

	DXTCreateBuffer(device, 2 * sizeof(XMFLOAT4X4), D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 
		D3D11_USAGE_DYNAMIC, &transformConstantBuffer);

	return S_OK;
}

void Renderer::Render(Scene * scene, DXTCameraBase * camera)
{
	FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)parameters.Extent.Width, (FLOAT)parameters.Extent.Height, 0.0f, 1.0f };
	context->ClearRenderTargetView(diffuseRenderTarget, clearColor);
	context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	context->OMSetDepthStencilState(depthStencilStateEnabled, 0);
	context->OMSetRenderTargets(1, &diffuseRenderTarget, depthStencilView);
	context->RSSetState(rasterizerState);
	context->RSSetViewports(1, &viewport);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(staticMeshVertexShader, nullptr, 0);
	context->PSSetShader(staticMeshPixelShader, nullptr, 0);

	for (auto& mesh : scene->Meshes)
	{
		D3D11_MAPPED_SUBRESOURCE subres;
		context->Map(transformConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres);

		context->Unmap(transformConstantBuffer, 0);
	}
}

void Renderer::Release()
{
	diffuseBuffer->Release();
	diffuseRenderTarget->Release();
	diffuseResourceView->Release();

	depthTexture->Release();
	depthStencilView->Release();
	backBufferRenderTarget->Release();
	rasterizerState->Release();
	depthStencilStateEnabled->Release();
	depthStencilStateDisabled->Release();
	blitSamplerState->Release();
	staticMeshSamplerState->Release();

	staticMeshVertexShader->Release();
	staticMeshPixelShader->Release();
	blitVertexShader->Release();
	blitPixelShader->Release();

	blitVertexBuffer->Release();
	blitInputLayout->Release();
	staticMeshInputLayout->Release();

	transformConstantBuffer->Release();

	context->Release();
	device->Release();
	swapChain->Release();
}