#include "DirectXToolbox.h"

using namespace DirectX;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int cmdShow)
{
	DXTInputHandlerDefault inputHandler;
	DXTWindowEventHandlerDefault eventHandler;
	DXTWindow window(hInstance, &inputHandler, &eventHandler);

	DXTRenderParams params;
	params.Extent = { 800, 600 };
	params.UseVSync = true;
	params.Windowed = true;

	HRESULT result;
	result = window.Initialize(params, "DXT Example (DirectX 11)");

	if (SUCCEEDED(result))
	{
		ID3D11Device* device;
		ID3D11DeviceContext* context;
		IDXGISwapChain* swapChain;
		result = DXTInitDevice(params, &window, &swapChain, &device, &context);

		if (SUCCEEDED(result))
		{
			eventHandler.SetSwapChain(swapChain);

			FLOAT clearColor[] = { 0.5f, 0.5f, 1.0f, 1.0f };
			D3D11_INPUT_ELEMENT_DESC inputDesc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 5, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			UINT elementCount = 3;
			UINT stride = 8 * sizeof(FLOAT);
			UINT offset = 0;
			UINT indexCount = 0;
			FLOAT deltaTime = 0.016f;

			DXTSphericalCamera camera;
			DXTFirstPersonCameraController cameraController(&camera, &inputHandler);
			inputHandler.AddInputInterface(&cameraController);
			cameraController.Velocity = 40.0f;
			cameraController.RotationVelocity = 0.005f;
			camera.Position = DirectX::XMFLOAT3(20.0f, 20.0f, 20.0f);
			camera.LookAt(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

			ID3D11RenderTargetView* renderTargetView;
			ID3D11Texture2D* depthBuffer;
			ID3D11DepthStencilView* depthBufferView;
			ID3D11VertexShader* vertexShader;
			ID3D11PixelShader* pixelShader;
			DXTBytecodeBlob vertexBytecode;
			ID3D11DepthStencilState* depthState;
			ID3D11RasterizerState* rasterizerState;
			ID3D11Buffer* vertexBuffer;
			ID3D11Buffer* indexBuffer;
			ID3D11InputLayout* inputLayout;
			ID3D11Buffer* transformBuffer;

			DXTCreateRenderTargetFromBackBuffer(swapChain, device, &renderTargetView);
			DXTCreateDepthStencilBuffer(device, params.Extent.Width, params.Extent.Height, DXGI_FORMAT_D24_UNORM_S8_UINT, &depthBuffer, &depthBufferView);
			DXTVertexShaderFromFile(device, "VertexShader.cso", &vertexShader, &vertexBytecode);
			DXTPixelShaderFromFile(device, "PixelShader.cso", &pixelShader);
			DXTCreateDepthStencilStateDepthTestEnabled(device, &depthState);
			DXTCreateRasterizerStateSolid(device, &rasterizerState);
			DXTLoadStaticMeshFromFile(device, "mesh.ase", DXTVertexAttributePosition | DXTVertexAttributeUV | DXTVertexAttributeNormal,
				DXTIndexTypeShort, &vertexBuffer, &indexBuffer, &indexCount);
			DXTCreateBuffer(device, sizeof(DirectX::XMFLOAT4X4) * 2, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, &transformBuffer);

			device->CreateInputLayout(inputDesc, elementCount, vertexBytecode.Bytecode, vertexBytecode.BytecodeLength, &inputLayout);
			vertexBytecode.Destroy();

			window.Present(false);

			while (!window.QuitMessageReceived())
			{
				window.MessagePump();

				if (inputHandler.IsKeyDown(VK_ESCAPE))
					break;

				cameraController.Update(deltaTime);

				XMFLOAT4X4 ViewProj;
				XMFLOAT4X4 World;
				camera.GetViewProjectionMatrix(&ViewProj, params.Extent);
				XMStoreFloat4x4(&World, XMMatrixIdentity());

				D3D11_MAPPED_SUBRESOURCE subres;
				context->Map(transformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subres);
				XMFLOAT4X4* ptr = (XMFLOAT4X4*)subres.pData;
				ptr[0] = World;
				ptr[1] = ViewProj;
				context->Unmap(transformBuffer, 0);

				D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)params.Extent.Width, (FLOAT)params.Extent.Height, 0.0f, 1.0f };
				context->ClearRenderTargetView(renderTargetView, clearColor);
				context->ClearDepthStencilView(depthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);
				context->OMSetDepthStencilState(depthState, 0);
				context->OMSetRenderTargets(1, &renderTargetView, depthBufferView);
				context->RSSetState(rasterizerState);
				context->RSSetViewports(1, &viewport);
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
				context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
				context->IASetInputLayout(inputLayout);
				context->VSSetShader(vertexShader, nullptr, 0);
				context->PSSetShader(pixelShader, nullptr, 0);
				context->VSSetConstantBuffers(0, 1, &transformBuffer);
				
				context->DrawIndexed(indexCount, 0, 0);

				swapChain->Present(1, 0);
			}

			swapChain->SetFullscreenState(false, nullptr);
			
			transformBuffer->Release();
			depthBufferView->Release();
			depthBuffer->Release();
			inputLayout->Release();
			vertexBuffer->Release();
			indexBuffer->Release();
			depthState->Release();
			rasterizerState->Release();
			vertexShader->Release();
			pixelShader->Release();
			renderTargetView->Release();
			swapChain->Release();
			context->Release();
			device->Release();
		}

		window.Destroy();
	}
}