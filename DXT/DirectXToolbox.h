#pragma once

#include <set>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

#define DXT_BLIT_VERTEX_COUNT 6

class DXTWindow;

struct DXTExtent2D
{
	int Width;
	int Height;
};

struct DXTBounds
{
	DirectX::XMFLOAT3 Lower;
	DirectX::XMFLOAT3 Upper;
};

struct DXTPlane
{
	DirectX::XMFLOAT3 Normal;
	float Distance;
};

struct DXTFrustum
{
	DXTPlane Planes[6];
};

struct DXTShadowCascadeInfo
{
	float NearPlane;
	float FarPlane;
};

struct DXTCameraShadowInfo
{
	std::vector<DXTShadowCascadeInfo> Cascades;
};

class DXTCameraBase
{
public:
	void GetViewProjectionMatrix(DirectX::XMFLOAT4X4* matrixOut, const DXTExtent2D& viewportSize);
	virtual void GetPosition(DirectX::XMFLOAT3* positionOut) = 0;
	virtual void GetViewDirection(DirectX::XMFLOAT3* directionOut) = 0;
	virtual void GetViewMatrix(DirectX::XMFLOAT4X4* matrixOut) = 0;
	virtual void GetProjectionMatrix(DirectX::XMFLOAT4X4* matrixOut, const DXTExtent2D& viewportSize) = 0;
	virtual void GetFrustum(DXTFrustum* frustum, const DXTExtent2D& viewportSize) = 0;
	virtual void GetFrustum(DXTFrustum* frustum, const DXTExtent2D& viewportSize, const float nearPlane, const float farPlane) = 0;
	virtual const DXTCameraShadowInfo* GetShadowInfo() const = 0;
};

class DXTSphericalCamera : public DXTCameraBase
{
public:
	DXTSphericalCamera();
	DXTSphericalCamera(const DirectX::XMFLOAT3& position, const float yaw, const float pitch,
		const float nearPlane, const float farPlane, const float fieldOfView);

	DXTCameraShadowInfo CascadeInfo;
	DirectX::XMFLOAT3 Position;

	float Yaw;
	float Pitch;

	float NearPlane;
	float FarPlane;
	float FieldOfView;

	void GetPosition(DirectX::XMFLOAT3* positionOut) override;
	void GetViewMatrix(DirectX::XMFLOAT4X4* matrixOut) override;
	void GetViewDirection(DirectX::XMFLOAT3* directionOut) override;
	void GetProjectionMatrix(DirectX::XMFLOAT4X4* matrixOut, const DXTExtent2D& viewportSize) override;
	void GetFrustum(DXTFrustum* frustum, const DXTExtent2D& viewportSize) override;
	void GetFrustum(DXTFrustum* frustum, const DXTExtent2D& viewportSize, const float nearPlane, const float farPlane) override;
	virtual const DXTCameraShadowInfo* GetShadowInfo() const override;

	void GetForward(DirectX::XMFLOAT3* vecOut);
	void LookAt(const float x, const float y, const float z);
	void LookAt(const DirectX::XMFLOAT3& target);
};

enum DXTMouseKey
{
	MOUSE_KEY_LEFT,
	MOUSE_KEY_RIGHT
};

struct DXTMouseEventArgs
{
	DXTMouseKey MouseKey;
	int MouseX;
	int MouseY;
};

struct DXTMouseMoveEventArgs
{
	int MouseX;
	int MouseY;
};

struct DXTKeyEventArgs
{
	WPARAM Key;
};

class DXTInputEventInterface
{
public:
	virtual void OnMouseDown(const DXTMouseEventArgs& mouseEvent) = 0;
	virtual void OnMouseUp(const DXTMouseEventArgs& mouseEvent) = 0;
	virtual void OnMouseMove(const DXTMouseMoveEventArgs& mouseEvent) = 0;
	virtual void OnKeyDown(const DXTKeyEventArgs& keyEvent) = 0;
	virtual void OnKeyUp(const DXTKeyEventArgs& keyEvent) = 0;
};

class DXTInputHandlerBase : public DXTInputEventInterface
{
private:
	std::set<WPARAM> pressedKeys;
	DXTWindow* window;

public:
	inline void SetWindow(DXTWindow* window);

	inline void SetMousePosition(const POINT& pos);
	inline POINT GetMousePosition() const;
	inline RECT GetClientSize() const;
	inline void GetMousePosition(LPPOINT posOut) const;
	inline void GetClientSize(LPRECT extentOut) const;
	inline int ShowMouse(bool bShow);

	inline bool IsKeyDown(const WPARAM key);
	inline bool IsKeyUp(const WPARAM key);

	inline void RegisterKey(const WPARAM key);
	inline void UnregisterKey(const WPARAM key);
};

class DXTFirstPersonCameraController : public DXTInputEventInterface
{
protected:
	DXTSphericalCamera* camera;
	DXTInputHandlerBase* inputHandler;

	POINT MouseCapturePosition;
	bool bHasCapturedMouse;

	void CenterMouse();

public:
	DXTFirstPersonCameraController(DXTSphericalCamera* camera, DXTInputHandlerBase* inputHandler);

	float RotationVelocity;
	float Velocity;
	WPARAM ForwardKey;
	WPARAM BackwardKey;
	WPARAM LeftKey;
	WPARAM RightKey;

	void OnMouseDown(const DXTMouseEventArgs& args) override;
	void OnMouseUp(const DXTMouseEventArgs& args) override;
	void OnMouseMove(const DXTMouseMoveEventArgs& mouseEvent) override;
	void OnKeyDown(const DXTKeyEventArgs& keyEvent) override;
	void OnKeyUp(const DXTKeyEventArgs& keyEvent) override;

	void Update(const float delta);
};

class DXTWindowEventHandlerBase
{
public:
	virtual void OnWindowMove() = 0;
	virtual void OnFullscreenExit() = 0;
	virtual void OnFullscreenReenter() = 0;
};

class DXTInputHandlerDefault : public DXTInputHandlerBase
{
private:
	std::vector<DXTInputEventInterface*> inputInterfaces;

public:
	void AddInputInterface(DXTInputEventInterface* obj);
	void ClearInterfaces();

	void OnMouseDown(const DXTMouseEventArgs& mouseEvent) override;
	void OnMouseUp(const DXTMouseEventArgs& mouseEvent) override;
	void OnMouseMove(const DXTMouseMoveEventArgs& mouseEvent) override;
	void OnKeyDown(const DXTKeyEventArgs& keyEvent) override;
	void OnKeyUp(const DXTKeyEventArgs& keyEvent) override;
};

class DXTWindowEventHandlerDefault : public DXTWindowEventHandlerBase
{
private:
	IDXGISwapChain* swapChain;

public:
	DXTWindowEventHandlerDefault();

	void SetSwapChain(IDXGISwapChain* chain);
	void OnWindowMove() override;
	void OnFullscreenExit() override;
	void OnFullscreenReenter() override;
};

struct DXTRenderParams
{
	DXTExtent2D Extent;
	bool UseVSync;
	bool Windowed;
};

struct DXTWindowProcLink
{
	DXTRenderParams* RenderParamsRef;
	DXTInputHandlerBase* WindowInputHandler;
	DXTWindowEventHandlerBase* WindowEventHandler;
	bool bIsMinimized;
};

class DXTWindow
{
private:
	HWND hWindow;
	HINSTANCE hParentInstance;
	DXTInputHandlerBase* inputHandler;
	DXTWindowEventHandlerBase* eventHandler;
	DXTRenderParams currentParams;
	DXTWindowProcLink procLink;
	bool bQuitReceived;
	MSG message;

public:
	DXTWindow(HINSTANCE instance);
	DXTWindow(HINSTANCE instance, DXTInputHandlerBase* inputhandlerInterface);
	DXTWindow(HINSTANCE instance, DXTInputHandlerBase* inputhandlerInterface, DXTWindowEventHandlerBase* eventHandlerInterface);

	HRESULT Initialize(const DXTRenderParams& renderParams, const char* title);
	void Present(const bool bHideCursor);
	void MessagePump();
	void Destroy();

	inline HWND GetWindowHandle() const;
	inline HINSTANCE GetInstanceHandle() const;
	inline bool QuitMessageReceived() const;
};

class DXTBytecodeBlob
{
public:
	void* Bytecode;
	size_t BytecodeLength;

	void Destroy();
};

enum DXTVertexAttrubuteChannel
{
	DXTVertexAttributePosition = 1 << 0,
	DXTVertexAttributeUV = 1 << 1,
	DXTVertexAttributeNormal = 1 << 2,
	DXTVertexAttributeTangent = 1 << 3,
	DXTVertexAttributeBitangent = 1 << 4
};

enum DXTIndexType
{
	DXTIndexTypeShort,
	DXTIndexTypeInt
};

void DXTConstructPlaneFromNormalAndPoint(const DirectX::XMVECTOR& point,
	const DirectX::XMVECTOR& normal, DXTPlane* planeOut);
void DXTConstructPlaneFromPoints(const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& p2,
	const DirectX::XMVECTOR& p3, DXTPlane* planeOut);
bool DXTIsOutsideFrustum(const DXTBounds& bounds, const DXTFrustum& frustum);
void DXTConstructFrustum(const float fieldOfView, const float farPlane, const float nearPlane,
	const DirectX::XMFLOAT3& cameraPosition, const DirectX::XMFLOAT3& cameraTarget,
	const DirectX::XMFLOAT3& cameraUp, const float aspectRatio, DXTFrustum* frustumOut);
void DXTTransformBounds(const DirectX::XMMATRIX& matrix, const DXTBounds& bounds, DXTBounds* boundsOut);

HRESULT DXTInitDevice(const DXTRenderParams& params, const DXTWindow* window, IDXGISwapChain** swapChainOut,
	ID3D11Device** deviceOut, ID3D11DeviceContext** deviceContextOut);
HRESULT DXTCreateRasterizerStateSolid(ID3D11Device* device, ID3D11RasterizerState** output);
HRESULT DXTCreateRasterizerStateWireframe(ID3D11Device* device, ID3D11RasterizerState** output);
HRESULT DXTCreateSamplerStateLinearClamp(ID3D11Device* device, ID3D11SamplerState** output);
HRESULT DXTCreateSamplerStateLinearWrap(ID3D11Device* device, ID3D11SamplerState** output);
HRESULT DXTCreateSamplerStatePointClamp(ID3D11Device* device, ID3D11SamplerState** output);
HRESULT DXTCreateDepthStencilStateDepthTestEnabled(ID3D11Device* device, ID3D11DepthStencilState** output);
HRESULT DXTCreateDepthStencilStateDepthTestDisabled(ID3D11Device* device, ID3D11DepthStencilState** output);
HRESULT DXTVertexShaderFromFile(ID3D11Device* device, const char* path, ID3D11VertexShader** output);
HRESULT DXTVertexShaderFromFile(ID3D11Device* device, const char* path, ID3D11VertexShader** output, DXTBytecodeBlob* bytecodeOutput);
HRESULT DXTPixelShaderFromFile(ID3D11Device* device, const char* path, ID3D11PixelShader** output);
HRESULT DXTPixelShaderFromFile(ID3D11Device* device, const char* path, ID3D11PixelShader** output, DXTBytecodeBlob* bytecodeOutput);
HRESULT DXTCreateBufferFromData(ID3D11Device* device, const void* data, const size_t dataLength, 
	const UINT bindFlags, const UINT cpuAccessFlags, const D3D11_USAGE usage, ID3D11Buffer** output);
HRESULT DXTCreateBuffer(ID3D11Device* device, const size_t length, const UINT bindFlags, 
	const UINT cpuAccessFlags, const D3D11_USAGE usage, ID3D11Buffer** output);
HRESULT DXTCreateDepthStencilBuffer(ID3D11Device* device, const size_t width, const size_t height,
	const DXGI_FORMAT format, ID3D11Texture2D** texture, ID3D11DepthStencilView** depthStencilView);
HRESULT DXTLoadStaticMeshFromFile(const char* path, const UINT channelFlags, const DXTIndexType indexType, 
	void** data, size_t* dataLength, void** indexData, size_t* indexDataLength, size_t* indexCount);
HRESULT DXTLoadStaticMeshFromFile(ID3D11Device* device, const char* path, const UINT channelFlags, const DXTIndexType indexType,
	ID3D11Buffer** vertexBuffer, ID3D11Buffer** indexBuffer, size_t* indexCount);
HRESULT DXTCreateBlitVertexBuffer(ID3D11Device* device, ID3D11Buffer** bufferOut);
HRESULT DXTCreateBlitInputLayout(ID3D11Device* device, DXTBytecodeBlob* vertexShaderCode, ID3D11InputLayout** inputLayoutOut);
HRESULT DXTCreateShadowMap(ID3D11Device* device, const size_t width, const size_t height, ID3D11Texture2D** texture,
	ID3D11DepthStencilView** depthStencilView, ID3D11ShaderResourceView** shaderResource);
HRESULT DXTCreateRenderTarget(ID3D11Device* device, const size_t width, const size_t height, 
	const DXGI_FORMAT format, ID3D11Texture2D** texture, ID3D11RenderTargetView** renderTargetView,
	ID3D11ShaderResourceView** shaderResourceView);
HRESULT DXTCreateRenderTargetFromBackBuffer(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11RenderTargetView** renderTargetView);

inline void DXTInputHandlerBase::GetMousePosition(LPPOINT posOut) const
{
	GetCursorPos(posOut);
	ScreenToClient(window->GetWindowHandle(), posOut);
}

inline void DXTInputHandlerBase::GetClientSize(LPRECT extentOut) const
{
	GetClientRect(window->GetWindowHandle(), extentOut);
}

inline int DXTInputHandlerBase::ShowMouse(bool bShow)
{
	return ShowCursor(static_cast<BOOL>(bShow));
}

inline void DXTInputHandlerBase::SetWindow(DXTWindow* windowValue)
{
	window = windowValue;
}

inline void DXTInputHandlerBase::SetMousePosition(const POINT& pos)
{
	POINT tempPos = pos;
	ClientToScreen(window->GetWindowHandle(), &tempPos);
	SetCursorPos(tempPos.x, tempPos.y);
}

inline RECT DXTInputHandlerBase::GetClientSize() const
{
	RECT rect;
	GetClientRect(window->GetWindowHandle(), &rect);
	return rect;
}

inline POINT DXTInputHandlerBase::GetMousePosition() const
{
	POINT pos;
	GetCursorPos(&pos);
	ScreenToClient(window->GetWindowHandle(), &pos);
	return pos;
}

inline void DXTInputHandlerBase::RegisterKey(const WPARAM key)
{
	pressedKeys.insert(key);
}

inline void DXTInputHandlerBase::UnregisterKey(const WPARAM key)
{
	pressedKeys.erase(key);
}

inline bool DXTInputHandlerBase::IsKeyDown(const WPARAM key)
{
	return pressedKeys.find(key) != pressedKeys.end();
}

inline bool DXTInputHandlerBase::IsKeyUp(const WPARAM key)
{
	return pressedKeys.find(key) == pressedKeys.end();
}

inline bool DXTWindow::QuitMessageReceived() const
{
	return bQuitReceived;
}

inline HWND DXTWindow::GetWindowHandle() const
{
	return hWindow;
}

inline HINSTANCE DXTWindow::GetInstanceHandle() const
{
	return hParentInstance;
}