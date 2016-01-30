#include "DirectXToolbox.h"

#include <windowsx.h>
#include <fstream>
#include <limits>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace std;
using namespace DirectX;

#define WINDOW_CLASS_NAME "RendererWindowClass"

#define DEFAULT_NEAR_PLANE 1.0f
#define DEFAULT_FAR_PLANE 500.0f
#define DEFAULT_FOV XM_PI / 3.0f

inline void DXTConstructPlaneFromNormalAndPoint(const DirectX::XMVECTOR& point,
	const DirectX::XMVECTOR& normal, DXTPlane* planeOut)
{
	XMStoreFloat3(&planeOut->Normal, normal);
	XMStoreFloat(&planeOut->Distance, XMVector3Dot(point, normal));
}

inline void DXTConstructPlaneFromPoints(const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& p2,
	const DirectX::XMVECTOR& p3, DXTPlane* planeOut)
{
	auto normal = XMVector3Cross(p2 - p1, p3 - p1);
	normal = XMVector3Normalize(normal);

	XMStoreFloat3(&planeOut->Normal, normal);
	XMStoreFloat(&planeOut->Distance, XMVector3Dot(normal, p1));
}

bool DXTIsOutsideFrustum(const DXTBounds& bounds, const DXTFrustum& frustum)
{
	XMFLOAT3 testPoints[] =
	{
		{ bounds.Lower.x, bounds.Lower.y, bounds.Lower.z },
		{ bounds.Lower.x, bounds.Lower.y, bounds.Upper.z },
		{ bounds.Lower.x, bounds.Upper.y, bounds.Lower.z },
		{ bounds.Lower.x, bounds.Upper.y, bounds.Upper.z },
		{ bounds.Upper.x, bounds.Lower.y, bounds.Lower.z },
		{ bounds.Upper.x, bounds.Lower.y, bounds.Upper.z },
		{ bounds.Upper.x, bounds.Upper.y, bounds.Lower.z },
		{ bounds.Upper.x, bounds.Upper.y, bounds.Upper.z }
	};

	for (int i = 0; i < 6; ++i)
	{
		int sum = 0;

		for (int j = 0; j < 8; ++j)
		{
			sum += ((testPoints[j].x * frustum.Planes[i].Normal.x +
				testPoints[j].y * frustum.Planes[i].Normal.y +
				testPoints[j].z * frustum.Planes[i].Normal.z) > frustum.Planes[i].Distance);

			if (sum == 8)
				return true;
		}
	}

	return false;
}

void DXTConstructFrustum(const float fieldOfView, const float farPlane, const float nearPlane,
	const DirectX::XMFLOAT3& cameraPosition, const DirectX::XMFLOAT3& cameraTarget,
	const DirectX::XMFLOAT3& cameraUp, const float aspectRatio, DXTFrustum* frustumOut)
{
	XMVECTOR position = XMLoadFloat3(&cameraPosition);
	XMVECTOR target = XMLoadFloat3(&cameraTarget);
	XMVECTOR up = XMLoadFloat3(&cameraUp);
	XMVECTOR forward = target - position;
	XMVECTOR left = XMVector3Cross(up, forward);

	up = XMVector3Cross(forward, left);
	forward = XMVector3Normalize(forward);
	left = XMVector3Normalize(left);
	up = XMVector3Normalize(up);

	XMVECTOR nearCenter = position + forward * nearPlane;
	XMVECTOR farCenter = position + forward * farPlane;

	float a = 2.0f * (float)tan(fieldOfView * 0.5f);
	float nearHeight = nearPlane * a;
	float farHeight = farPlane * a;
	float nearWidth = aspectRatio * nearHeight;
	float farWidth = aspectRatio * farHeight;

	XMVECTOR farTopLeft = farCenter + 0.5f * farWidth * left + 0.5f * farHeight * up;
	XMVECTOR farBottomLeft = farTopLeft - farHeight * up;
	XMVECTOR farTopRight = farTopLeft - farWidth * left;
	XMVECTOR farBottomRight = farTopRight - farHeight * up;

	XMVECTOR nearTopLeft = nearCenter + 0.5f * nearWidth * left + 0.5f * nearHeight * up;
	XMVECTOR nearBottomLeft = nearTopLeft - nearHeight * up;
	XMVECTOR nearTopRight = nearTopLeft - nearWidth * left;
	XMVECTOR nearBottomRight = nearTopRight - nearHeight * up;

	// Front plane
	DXTConstructPlaneFromNormalAndPoint(nearCenter, -forward, &frustumOut->Planes[0]);

	// Back plane
	DXTConstructPlaneFromNormalAndPoint(farCenter, forward, &frustumOut->Planes[1]);

	// Top plane
	DXTConstructPlaneFromPoints(farTopLeft, nearTopLeft, farTopRight, &frustumOut->Planes[2]);

	// Bottom plane
	DXTConstructPlaneFromPoints(farBottomLeft, farBottomRight, nearBottomLeft, &frustumOut->Planes[3]);

	// Left plane
	DXTConstructPlaneFromPoints(farBottomLeft, nearBottomLeft, farTopLeft, &frustumOut->Planes[4]);

	// Right plane
	DXTConstructPlaneFromPoints(farBottomRight, farTopRight, nearBottomRight, &frustumOut->Planes[5]);
}

void DXTTransformBounds(const XMMATRIX& matrix, const DXTBounds& bounds, DXTBounds* boundsOut)
{
	XMVECTOR vecs[8] =
	{
		XMVectorSet(bounds.Lower.x, bounds.Lower.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Lower.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Upper.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Upper.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Lower.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Lower.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Upper.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Upper.y, bounds.Upper.z, 1.0f)
	};

	auto infinity = numeric_limits<float>::infinity();
	boundsOut->Lower = { infinity, infinity, infinity };
	boundsOut->Upper = { -infinity, -infinity, -infinity };

	XMFLOAT3 vecResult;

	for (size_t i = 0; i < 8; ++i)
	{
		vecs[i] = XMVector4Transform(vecs[i], matrix);
		XMStoreFloat3(&vecResult, vecs[i]);

		if (boundsOut->Lower.x > vecResult.x)
			boundsOut->Lower.x = vecResult.x;
		if (boundsOut->Lower.y > vecResult.y)
			boundsOut->Lower.y = vecResult.y;
		if (boundsOut->Lower.z > vecResult.z)
			boundsOut->Lower.z = vecResult.z;

		if (boundsOut->Upper.x < vecResult.x)
			boundsOut->Upper.x = vecResult.x;
		if (boundsOut->Upper.y < vecResult.y)
			boundsOut->Upper.y = vecResult.y;
		if (boundsOut->Upper.z < vecResult.z)
			boundsOut->Upper.z = vecResult.z;
	}
}

void DXTSphericalCamera::GetForward(XMFLOAT3* vecOut)
{
	vecOut->x = static_cast<float>(cos(Yaw) * sin(Pitch));
	vecOut->y = static_cast<float>(cos(Pitch));
	vecOut->z = static_cast<float>(sin(Yaw)* sin(Pitch));
}

void DXTSphericalCamera::GetPosition(DirectX::XMFLOAT3* positionOut)
{
	*positionOut = Position;
}

void DXTSphericalCamera::GetViewMatrix(XMFLOAT4X4* matrixOut)
{
	XMVECTOR eye = XMLoadFloat3(&Position);
	XMVECTOR target = XMVectorSet(static_cast<float>(cos(Yaw) * sin(Pitch)),
		static_cast<float>(cos(Pitch)),
		static_cast<float>(sin(Yaw) * sin(Pitch)),
		0.0f);
	target += eye;
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMStoreFloat4x4(matrixOut, XMMatrixLookAtLH(eye, target, up));
}

void DXTSphericalCamera::GetViewDirection(DirectX::XMFLOAT3 * directionOut)
{
	directionOut->x = static_cast<float>(cos(Yaw) * sin(Pitch));
	directionOut->y = static_cast<float>(cos(Pitch));
	directionOut->z = static_cast<float>(sin(Yaw) * sin(Pitch));
}

void DXTSphericalCamera::GetProjectionMatrix(XMFLOAT4X4* matrixOut, const DXTExtent2D& viewportSize)
{
	float aspectRatio = (float)viewportSize.Width / (float)viewportSize.Height;
	XMStoreFloat4x4(matrixOut, XMMatrixPerspectiveFovLH(FieldOfView, aspectRatio, NearPlane, FarPlane));
}

void DXTSphericalCamera::LookAt(const float x, const float y, const float z)
{
	LookAt(XMFLOAT3(x, y, z));
}

void DXTSphericalCamera::LookAt(const XMFLOAT3& target)
{
	XMVECTOR eye = XMLoadFloat3(&Position);
	XMVECTOR targetVec = XMLoadFloat3(&target);
	XMVECTOR directionVec = targetVec - eye;
	directionVec = XMVector3Normalize(directionVec);
	XMFLOAT3 direction;
	XMStoreFloat3(&direction, directionVec);

	Yaw = static_cast<float>(atan2(direction.z, direction.x));
	Pitch = static_cast<float>(acos(direction.y));
}

void DXTSphericalCamera::GetFrustum(DXTFrustum* frustum, const DXTExtent2D& viewportSize)
{
	GetFrustum(frustum, viewportSize, NearPlane, FarPlane);
}

void DXTSphericalCamera::GetFrustum(DXTFrustum * frustum, const DXTExtent2D & viewportSize, const float nearPlane, const float farPlane)
{
	XMFLOAT3 forward;
	GetForward(&forward);
	XMFLOAT3 target;
	XMStoreFloat3(&target, XMLoadFloat3(&forward) + XMLoadFloat3(&Position));

	float aspectRatio = (float)viewportSize.Width / (float)viewportSize.Height;

	DXTConstructFrustum(FieldOfView, farPlane, nearPlane,
		Position, target, XMFLOAT3(0.0f, 1.0f, 0.0f),
		aspectRatio, frustum);
}

const DXTCameraShadowInfo* DXTSphericalCamera::GetShadowInfo() const
{
	return &CascadeInfo;
}

DXTSphericalCamera::DXTSphericalCamera() :
	NearPlane(DEFAULT_NEAR_PLANE),
	FarPlane(DEFAULT_FAR_PLANE),
	FieldOfView(DEFAULT_FOV),
	Position(0.0f, 0.0f, 0.0f),
	Yaw(0.0f),
	Pitch(XM_PI / 2.0f)
{
}

DXTSphericalCamera::DXTSphericalCamera(const XMFLOAT3& position, const float yaw, const float pitch,
	const float nearPlane, const float farPlane, const float fieldOfView) :
	NearPlane(nearPlane),
	FarPlane(farPlane),
	FieldOfView(fieldOfView),
	Position(position),
	Yaw(yaw),
	Pitch(pitch)
{
}

DXTFirstPersonCameraController::DXTFirstPersonCameraController(DXTSphericalCamera* camera,
	DXTInputHandlerBase* inputHandler) :
	camera(camera),
	inputHandler(inputHandler),
	bHasCapturedMouse(false),
	RotationVelocity(0.0f),
	Velocity(0.0f),
	ForwardKey('W'),
	BackwardKey('S'),
	LeftKey('A'),
	RightKey('D')
{
}

void DXTFirstPersonCameraController::CenterMouse()
{
	RECT rect;
	inputHandler->GetClientSize(&rect);
	POINT pos = { rect.right / 2, rect.bottom / 2 };
	inputHandler->SetMousePosition(pos);
}

void DXTFirstPersonCameraController::OnMouseDown(const DXTMouseEventArgs& args)
{
	if (args.MouseKey == MOUSE_KEY_RIGHT)
	{
		inputHandler->ShowMouse(false);
		bHasCapturedMouse = true;
		MouseCapturePosition = inputHandler->GetMousePosition();
		CenterMouse();
	}
}

void DXTFirstPersonCameraController::OnMouseUp(const DXTMouseEventArgs& args)
{
	if (args.MouseKey == MOUSE_KEY_RIGHT)
	{
		bHasCapturedMouse = false;
		inputHandler->SetMousePosition(MouseCapturePosition);
		inputHandler->ShowMouse(true);
	}
}

void DXTFirstPersonCameraController::OnMouseMove(const DXTMouseMoveEventArgs & mouseEvent)
{
}

void DXTFirstPersonCameraController::OnKeyDown(const DXTKeyEventArgs & keyEvent)
{
}

void DXTFirstPersonCameraController::OnKeyUp(const DXTKeyEventArgs & keyEvent)
{
}

void DXTFirstPersonCameraController::Update(const float delta)
{
	if (bHasCapturedMouse)
	{
		POINT pos;
		RECT rect;
		inputHandler->GetMousePosition(&pos);
		inputHandler->GetClientSize(&rect);

		CenterMouse();

		auto dx = static_cast<float>(pos.x - rect.right / 2);
		auto dy = static_cast<float>(pos.y - rect.bottom / 2);

		camera->Yaw -= dx * RotationVelocity;
		camera->Pitch += dy * RotationVelocity;

		camera->Yaw = fmod(camera->Yaw, XM_2PI);
		camera->Pitch = min(XM_PI - 0.1f, max(0.1f, camera->Pitch));
	}

	XMFLOAT3 forwardDirection;
	camera->GetForward(&forwardDirection);
	XMVECTOR forwardDirectionVec = XMLoadFloat3(&forwardDirection);
	XMVECTOR position = XMLoadFloat3(&camera->Position);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR sideVec = XMVector3Cross(up, forwardDirectionVec);
	sideVec = XMVector3Normalize(sideVec);

	if (inputHandler->IsKeyDown(ForwardKey))
		position += forwardDirectionVec * Velocity * delta;
	if (inputHandler->IsKeyDown(BackwardKey))
		position -= forwardDirectionVec * Velocity * delta;
	if (inputHandler->IsKeyDown(LeftKey))
		position -= sideVec * Velocity * delta;
	if (inputHandler->IsKeyDown(RightKey))
		position += sideVec * Velocity * delta;

	XMStoreFloat3(&camera->Position, position);
}

LRESULT CALLBACK WindowProc(HWND hWindow, UINT message, WPARAM wparam, LPARAM lparam)
{
	DXTWindowProcLink* winObjects = reinterpret_cast<DXTWindowProcLink*>(GetWindowLongPtr(hWindow,
		GWLP_USERDATA));
	DXTInputHandlerBase* inputHandler = nullptr;
	DXTWindowEventHandlerBase* eventHandler = nullptr;
	DXTRenderParams* renderParams = nullptr;

	if (winObjects != nullptr)
	{
		inputHandler = winObjects->WindowInputHandler;
		eventHandler = winObjects->WindowEventHandler;
		renderParams = winObjects->RenderParamsRef;
	}

	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}

	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		SetCapture(hWindow);

		if (inputHandler != nullptr)
		{
			DXTMouseEventArgs args;
			args.MouseKey = MOUSE_KEY_LEFT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseDown(args);
		}
		return 0;
	}

	case WM_LBUTTONUP:
	{
		ReleaseCapture();

		if (inputHandler != nullptr)
		{
			DXTMouseEventArgs args;
			args.MouseKey = MOUSE_KEY_LEFT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseUp(args);
		}
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		SetCapture(hWindow);

		if (inputHandler != nullptr)
		{
			DXTMouseEventArgs args;
			args.MouseKey = MOUSE_KEY_RIGHT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseDown(args);
		}
		return 0;
	}

	case WM_RBUTTONUP:
	{
		ReleaseCapture();

		if (inputHandler != nullptr)
		{
			DXTMouseEventArgs args;
			args.MouseKey = MOUSE_KEY_RIGHT;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseUp(args);
		}
		return 0;
	}

	case WM_MOUSEMOVE:
	{
		if (inputHandler != nullptr)
		{
			DXTMouseMoveEventArgs args;
			args.MouseX = GET_X_LPARAM(lparam);
			args.MouseY = GET_Y_LPARAM(lparam);
			inputHandler->OnMouseMove(args);
		}
		return 0;
	}

	case WM_KEYDOWN:
	{
		if (inputHandler != nullptr)
		{
			inputHandler->RegisterKey(wparam);

			DXTKeyEventArgs args;
			args.Key = wparam;
			inputHandler->OnKeyDown(args);
		}
		return 0;
	}

	case WM_KEYUP:
	{
		if (inputHandler != nullptr)
		{
			inputHandler->UnregisterKey(wparam);

			DXTKeyEventArgs args;
			args.Key = wparam;
			inputHandler->OnKeyUp(args);
		}
		return 0;
	}

	case WM_KILLFOCUS:
	{
		if (renderParams != nullptr && !renderParams->Windowed)
		{
			winObjects->bIsMinimized = true;
			ShowWindow(hWindow, SW_MINIMIZE);
			eventHandler->OnFullscreenExit();
		}

		return 0;
	}

	case WM_SETFOCUS:
	{
		if (winObjects != nullptr && winObjects->bIsMinimized)
		{
			winObjects->bIsMinimized = false;
			eventHandler->OnFullscreenReenter();
		}
	}

	case WM_MOVE:
	{
		if (eventHandler != nullptr)
			eventHandler->OnWindowMove();

		return 0;
	}

	default:
		return DefWindowProc(hWindow, message, wparam, lparam);
	}
}

DXTWindow::DXTWindow(HINSTANCE instance) :
	hParentInstance(instance), inputHandler(nullptr), eventHandler(nullptr), bQuitReceived(false)
{
}

DXTWindow::DXTWindow(HINSTANCE instance, DXTInputHandlerBase * handlerInterface) :
	hParentInstance(instance), inputHandler(handlerInterface), eventHandler(nullptr), bQuitReceived(false)
{
	inputHandler->SetWindow(this);
}

DXTWindow::DXTWindow(HINSTANCE instance, DXTInputHandlerBase * handlerInterface, DXTWindowEventHandlerBase * eventHandlerInterface) :
	hParentInstance(instance), inputHandler(handlerInterface), eventHandler(eventHandlerInterface), bQuitReceived(false)
{
	inputHandler->SetWindow(this);
}

HRESULT DXTWindow::Initialize(const DXTRenderParams& renderParams, const char* title)
{
	WNDCLASSEX wc;
	DEVMODE dmScreenSettings;
	int posX, posY;

	OutputDebugString("Initializing Window...\n");

	// Setup the windows class with default settings.
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = &WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hParentInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = WINDOW_CLASS_NAME;
	wc.cbSize = sizeof(WNDCLASSEX);

	OutputDebugString("Registering Window Class...\n");

	// Register the window class.
	RegisterClassEx(&wc);

	int windowWidth = renderParams.Extent.Width;
	int windowHeight = renderParams.Extent.Height;

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	if (!renderParams.Windowed)
	{
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = (unsigned long)GetSystemMetrics(SM_CXSCREEN);
		dmScreenSettings.dmPelsHeight = (unsigned long)GetSystemMetrics(SM_CXSCREEN);
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

		// Set the position of the window to the top left corner.
		posX = 0;
		posY = 0;
	}
	else
	{
		// Place the window in the middle of the screen.
		posX = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
		posY = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

		RECT r = { 0, 0, windowWidth, windowHeight };
		AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

		windowWidth = r.right - r.left;
		windowHeight = r.bottom - r.top;
	}

	DWORD windowStyle = 0;
	if (!renderParams.Windowed)
		windowStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;
	else
		windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	OutputDebugString("Creating Window...\n");

	// Create the window with the screen settings and get the handle to it.
	hWindow = CreateWindowEx(WS_EX_APPWINDOW, WINDOW_CLASS_NAME, title,
		windowStyle,
		posX, posY, windowWidth, windowHeight, nullptr, nullptr, hParentInstance, nullptr);
	ShowWindow(hWindow, SW_HIDE);

	if (hWindow == nullptr)
		return E_FAIL;

	currentParams = renderParams;

	// Performing linking
	procLink.WindowInputHandler = inputHandler;
	procLink.WindowEventHandler = eventHandler;
	procLink.RenderParamsRef = &currentParams;
	procLink.bIsMinimized = false;
	SetWindowLongPtr(hWindow, GWLP_USERDATA, (LONG_PTR)&procLink);

	return S_OK;
}

void DXTWindow::Present(const bool bHideCursor)
{
	// Bring the window up on the screen and set it as main focus.
	ShowWindow(hWindow, SW_SHOW);
	SetForegroundWindow(hWindow);
	SetFocus(hWindow);

	// Hide the mouse cursor.
	if (bHideCursor)
		ShowCursor(false);
}

void DXTWindow::Destroy()
{
	ShowCursor(true);

	if (!currentParams.Windowed)
		ChangeDisplaySettings(nullptr, 0);

	DestroyWindow(hWindow);

	UnregisterClass(WINDOW_CLASS_NAME, hParentInstance);
}

void DXTWindow::MessagePump()
{
	while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			ShowWindow(hWindow, SW_HIDE);
			bQuitReceived = true;
		}
	}
}

HRESULT DXTInitDevice(const DXTRenderParams& params, const DXTWindow* window, IDXGISwapChain** swapChainOut,
	ID3D11Device** deviceOut, ID3D11DeviceContext** deviceContextOut)
{
	IDXGIFactory* factory;
	HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

	IDXGIAdapter* adapter;
	result = factory->EnumAdapters(0, &adapter);

	IDXGIOutput* adapterOutput;
	result = adapter->EnumOutputs(0, &adapterOutput);

	UINT modeCount;
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_ENUM_MODES_INTERLACED, &modeCount, nullptr);

	DXGI_MODE_DESC* modeDescriptions = new DXGI_MODE_DESC[modeCount];
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_ENUM_MODES_INTERLACED, &modeCount, modeDescriptions);

	DXGI_MODE_DESC* descMatch = nullptr;

	for (UINT i = 0; i < modeCount; ++i)
	{
		DXGI_MODE_DESC* desc = &modeDescriptions[i];

		if (desc->Width == params.Extent.Width && desc->Height == params.Extent.Height)
		{
			OutputDebugString("Found compatible display mode!\n");
			descMatch = desc;
			break;
		}
	}

	if (descMatch == nullptr)
	{
		OutputDebugString("No DXGI mode match found - using a default!\n");
		descMatch = modeDescriptions;
	}

	DXGI_ADAPTER_DESC adapterDesc;
	result = adapter->GetDesc(&adapterDesc);

	adapterOutput->Release();
	adapter->Release();
	factory->Release();

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	swapChainDesc.Windowed = params.Windowed;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc = *descMatch;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = window->GetWindowHandle();
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	delete[] modeDescriptions;

	UINT deviceCreationFlags = 0;
#ifdef ENABLE_DIRECT3D_DEBUG
	deviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	if (!params.Windowed)
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	OutputDebugString("Creating device and swap chain...\n");

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		deviceCreationFlags, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc,
		swapChainOut, deviceOut, nullptr, deviceContextOut);

	if (FAILED(result))
	{
		OutputDebugString("Failed to create device and swap chain!\n");
		return E_FAIL;
	}

	OutputDebugString("Device and swap chain created successfully!\n");

	return S_OK;
}

HRESULT DXTCreateRasterizerStateSolid(ID3D11Device * device, ID3D11RasterizerState ** output)
{
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	return device->CreateRasterizerState(&rasterDesc, output);
}

HRESULT DXTCreateRasterizerStateWireframe(ID3D11Device * device, ID3D11RasterizerState ** output)
{
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	return device->CreateRasterizerState(&rasterDesc, output);
}

HRESULT DXTCreateSamplerStateLinearClamp(ID3D11Device * device, ID3D11SamplerState ** output)
{
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return device->CreateSamplerState(&samplerDesc, output);
}

HRESULT DXTCreateSamplerStateLinearWrap(ID3D11Device * device, ID3D11SamplerState ** output)
{
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return device->CreateSamplerState(&samplerDesc, output);
}

HRESULT DXTCreateSamplerStatePointClamp(ID3D11Device * device, ID3D11SamplerState ** output)
{
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return device->CreateSamplerState(&samplerDesc, output);
}

HRESULT DXTCreateDepthStencilStateDepthTestEnabled(ID3D11Device * device, ID3D11DepthStencilState ** output)
{
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	return device->CreateDepthStencilState(&depthStencilDesc, output);
}

HRESULT DXTCreateDepthStencilStateDepthTestDisabled(ID3D11Device * device, ID3D11DepthStencilState ** output)
{
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	return device->CreateDepthStencilState(&depthStencilDesc, output);
}

HRESULT DXTVertexShaderFromFile(ID3D11Device* device, const char* path, ID3D11VertexShader** output)
{
	DXTBytecodeBlob blob;
	HRESULT result = DXTVertexShaderFromFile(device, path, output, &blob);
	blob.Destroy();
	return result;
}

HRESULT DXTVertexShaderFromFile(ID3D11Device* device, const char* path, ID3D11VertexShader** output, DXTBytecodeBlob* bytecodeOutput)
{
	OutputDebugString("Loading resource ");
	OutputDebugString(path);
	OutputDebugString("\n");

	char* bytecode = nullptr;
	size_t bytecodeLength;
	std::ifstream t(path, std::ios::binary | std::ios::in);
	if (t.fail())
		return E_FAIL;

	t.seekg(0, std::ios::end);
	bytecodeLength = static_cast<size_t>(t.tellg());
	t.seekg(0, std::ios::beg);

	bytecode = new char[bytecodeLength];
	t.read(bytecode, bytecodeLength);
	t.close();

	HRESULT result = device->CreateVertexShader(bytecode, bytecodeLength, nullptr, output);
	bytecodeOutput->Bytecode = bytecode;
	bytecodeOutput->BytecodeLength = bytecodeLength;

	return result;
}

HRESULT DXTPixelShaderFromFile(ID3D11Device * device, const char * path, ID3D11PixelShader ** output)
{
	DXTBytecodeBlob blob;
	HRESULT result = DXTPixelShaderFromFile(device, path, output, &blob);
	blob.Destroy();
	return result;
}

HRESULT DXTPixelShaderFromFile(ID3D11Device* device, const char* path, ID3D11PixelShader** output, DXTBytecodeBlob* bytecodeOutput)
{
	OutputDebugString("Loading resource ");
	OutputDebugString(path);
	OutputDebugString("\n");

	char* bytecode = nullptr;
	size_t bytecodeLength;
	std::ifstream t(path, std::ios::binary | std::ios::in);
	if (t.fail())
		return E_FAIL;

	t.seekg(0, std::ios::end);
	bytecodeLength = static_cast<size_t>(t.tellg());
	t.seekg(0, std::ios::beg);

	bytecode = new char[bytecodeLength];
	t.read(bytecode, bytecodeLength);
	t.close();

	HRESULT result = device->CreatePixelShader(bytecode, bytecodeLength, nullptr, output);
	bytecodeOutput->Bytecode = bytecode;
	bytecodeOutput->BytecodeLength = bytecodeLength;

	return result;
}

HRESULT DXTCreateBufferFromData(ID3D11Device * device, const void * data, const size_t dataLength, const UINT bindFlags, const UINT cpuAccessFlags, const D3D11_USAGE usage, ID3D11Buffer ** output)
{
	D3D11_BUFFER_DESC desc;
	desc.BindFlags = bindFlags;
	desc.ByteWidth = dataLength;
	desc.CPUAccessFlags = cpuAccessFlags;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	desc.Usage = usage;

	D3D11_SUBRESOURCE_DATA subData;
	ZeroMemory(&subData, sizeof(subData));
	subData.pSysMem = data;

	return device->CreateBuffer(&desc, &subData, output);
}

HRESULT DXTCreateBuffer(ID3D11Device * device, const size_t length, const UINT bindFlags, 
	const UINT cpuAccessFlags, const D3D11_USAGE usage, ID3D11Buffer ** output)
{
	D3D11_BUFFER_DESC desc;
	desc.BindFlags = bindFlags;
	desc.ByteWidth = length;
	desc.CPUAccessFlags = cpuAccessFlags;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	desc.Usage = usage;

	return device->CreateBuffer(&desc, nullptr, output);
}

HRESULT DXTCreateDepthStencilBuffer(ID3D11Device * device, const size_t width, const size_t height, 
	const DXGI_FORMAT format, ID3D11Texture2D ** texture, ID3D11DepthStencilView ** depthStencilView)
{
	D3D11_TEXTURE2D_DESC desc;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Width = width;
	desc.Height = height;
	desc.Format = format;

	HRESULT result = device->CreateTexture2D(&desc, nullptr, texture);
	if (FAILED(result))
		return result;

	D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
	ZeroMemory(&viewDesc, sizeof(viewDesc));
	viewDesc.Format = format;
	viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipSlice = 0;

	return device->CreateDepthStencilView(*texture, &viewDesc, depthStencilView);
}

HRESULT DXTLoadStaticMeshFromFile(const char * path, const UINT channelFlags, const DXTIndexType indexType, 
	void ** data, size_t * dataLength, void ** indexData, size_t * indexDataLength, size_t* indexCount)
{
	if (channelFlags == 0)
		return E_FAIL;

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path,
		aiProcessPreset_TargetRealtime_Fast | aiProcess_PreTransformVertices);

	if (!scene)
		return E_FAIL;

	int positionOffset = 0;
	int uvOffset = positionOffset + (channelFlags & DXTVertexAttributePosition ? 3 : 0);
	int normalOffset = uvOffset + (channelFlags & DXTVertexAttributeUV ? 2 : 0);
	int tangentOffset = normalOffset + (channelFlags & DXTVertexAttributeNormal ? 3 : 0);
	int bitangentOffset = tangentOffset + (channelFlags & DXTVertexAttributeTangent ? 3 : 0);
	int stride = bitangentOffset + (channelFlags & DXTVertexAttributeBitangent ? 3 : 0);

	if (scene->mNumMeshes == 0)
		return E_FAIL;

	if (scene->mNumMeshes > 1)
		OutputDebugString("Warning: more than one mesh found... DXTLoadStaticMeshFromFile will only take the first one.");

	auto mesh = scene->mMeshes[0];
	auto vertexDataSize = stride * mesh->mNumVertices;
	auto vertexData = new FLOAT[vertexDataSize];

	if (channelFlags & DXTVertexAttributePosition)
		for (size_t i = 0, loc = positionOffset; i < mesh->mNumVertices; ++i, loc += stride)
		{
			vertexData[loc] = mesh->mVertices[i].x;
			vertexData[loc + 1] = mesh->mVertices[i].y;
			vertexData[loc + 2] = mesh->mVertices[i].z;
		}
	if ((channelFlags & DXTVertexAttributeUV) && mesh->HasTextureCoords(0))
		for (size_t i = 0, loc = uvOffset; i < mesh->mNumVertices; ++i, loc += stride)
		{
			vertexData[loc] = mesh->mTextureCoords[0][i].x;
			vertexData[loc + 1] = mesh->mTextureCoords[0][i].y;
		}
	if ((channelFlags & DXTVertexAttributeNormal) && mesh->HasNormals())
		for (size_t i = 0, loc = normalOffset; i < mesh->mNumVertices; ++i, loc += stride)
		{
			vertexData[loc] = mesh->mNormals[i].x;
			vertexData[loc + 1] = mesh->mNormals[i].y;
			vertexData[loc + 2] = mesh->mNormals[i].z;
		}
	if ((channelFlags & DXTVertexAttributeTangent) && mesh->HasTangentsAndBitangents())
		for (size_t i = 0, loc = tangentOffset; i < mesh->mNumVertices; ++i, loc += stride)
		{
			vertexData[loc] = mesh->mTangents[i].x;
			vertexData[loc + 1] = mesh->mTangents[i].y;
			vertexData[loc + 2] = mesh->mTangents[i].z;
		}
	if ((channelFlags & DXTVertexAttributeBitangent) && mesh->HasTangentsAndBitangents())
		for (size_t i = 0, loc = bitangentOffset; i < mesh->mNumVertices; ++i, loc += stride)
		{
			vertexData[loc] = mesh->mBitangents[i].x;
			vertexData[loc + 1] = mesh->mBitangents[i].y;
			vertexData[loc + 2] = mesh->mBitangents[i].z;
		}

	*data = vertexData;
	*dataLength = vertexDataSize * sizeof(float);

	auto indexDataSize = mesh->mNumFaces * 3;
	*indexCount = indexDataSize;

	if (indexType == DXTIndexTypeShort)
	{
		UINT16* indexData16 = new UINT16[indexDataSize];
		for (size_t i = 0, loc = 0; i < mesh->mNumFaces; ++i)
		{
			indexData16[loc++] = mesh->mFaces[i].mIndices[0];
			indexData16[loc++] = mesh->mFaces[i].mIndices[1];
			indexData16[loc++] = mesh->mFaces[i].mIndices[2];
		}
		*indexData = indexData16;
		*indexDataLength = indexDataSize * sizeof(UINT16);
	}
	if (indexType == DXTIndexTypeInt)
	{
		UINT* indexData32 = new UINT[indexDataSize];
		for (size_t i = 0, loc = 0; i < mesh->mNumFaces; ++i)
		{
			indexData32[loc++] = mesh->mFaces[i].mIndices[0];
			indexData32[loc++] = mesh->mFaces[i].mIndices[1];
			indexData32[loc++] = mesh->mFaces[i].mIndices[2];
		}
		*indexData = indexData32;
		*indexDataLength = indexDataSize * sizeof(UINT);
	}

	return S_OK;
}

HRESULT DXTLoadStaticMeshFromFile(ID3D11Device * device, const char* path, const UINT channelFlags, 
	const DXTIndexType indexType, ID3D11Buffer ** vertexBuffer, ID3D11Buffer ** indexBuffer, size_t* indexCount)
{
	void* data = nullptr;
	size_t dataLength;
	void* indexData = nullptr;
	size_t indexDataLength;
	HRESULT result1 = DXTLoadStaticMeshFromFile(path, channelFlags, indexType, &data, &dataLength, &indexData, &indexDataLength, indexCount);

	if (FAILED(result1))
		return result1;

	HRESULT result2 = DXTCreateBufferFromData(device, data, dataLength, D3D11_BIND_VERTEX_BUFFER, 0, D3D11_USAGE_IMMUTABLE, vertexBuffer);
	HRESULT result3 = DXTCreateBufferFromData(device, indexData, indexDataLength, D3D11_BIND_INDEX_BUFFER, 0, D3D11_USAGE_IMMUTABLE, indexBuffer);

	delete[] data;
	delete[] indexData;

	if (FAILED(result2))
		return result2;
	return result3;
}

HRESULT DXTCreateBlitVertexBuffer(ID3D11Device * device, ID3D11Buffer** bufferOut)
{
	float blitBufferData[] =
	{
		-1.0f, 1.0f,
		0.0f, 0.0f,

		1.0f, 1.0f,
		1.0f, 0.0f,

		-1.0f, -1.0f,
		0.0f, 1.0f,

		-1.0f, -1.0f,
		0.0f, 1.0f,

		1.0f, 1.0f,
		1.0f, 0.0f,

		1.0f, -1.0f,
		1.0f, 1.0f
	};

	return DXTCreateBufferFromData(device, blitBufferData, sizeof(blitBufferData), D3D11_BIND_VERTEX_BUFFER, 0, D3D11_USAGE_IMMUTABLE, bufferOut);
}

HRESULT DXTCreateBlitInputLayout(ID3D11Device * device, DXTBytecodeBlob* vertexShaderCode, ID3D11InputLayout** inputLayoutOut)
{
	D3D11_INPUT_ELEMENT_DESC inputDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 2, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	return device->CreateInputLayout(inputDesc, 2, vertexShaderCode->Bytecode, vertexShaderCode->BytecodeLength, inputLayoutOut);
}

HRESULT DXTCreateShadowMap(ID3D11Device * device, const size_t width, const size_t height, ID3D11Texture2D ** texture, ID3D11DepthStencilView ** depthStencilView, ID3D11ShaderResourceView ** shaderResource)
{
	D3D11_TEXTURE2D_DESC desc;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.Height = height;
	desc.Width = width;
	desc.MipLevels = 1;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	HRESULT result = device->CreateTexture2D(&desc, nullptr, texture);
	if (FAILED(result))
		return result;

	D3D11_DEPTH_STENCIL_VIEW_DESC dDesc;
	ZeroMemory(&dDesc, sizeof(dDesc));
	dDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dDesc.Texture2D.MipSlice = 0;
	dDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	result = device->CreateDepthStencilView(*texture, &dDesc, depthStencilView);
	if (FAILED(result))
		return result;

	D3D11_SHADER_RESOURCE_VIEW_DESC sDesc;
	ZeroMemory(&sDesc, sizeof(sDesc));
	sDesc.Format = DXGI_FORMAT_R32_FLOAT;
	sDesc.Texture2D.MipLevels = 1;
	sDesc.Texture2D.MostDetailedMip = 0;
	sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

	result = device->CreateShaderResourceView(*texture, &sDesc, shaderResource);
	return result;
}

HRESULT DXTCreateRenderTarget(ID3D11Device * device, const size_t width, const size_t height, const DXGI_FORMAT format, ID3D11Texture2D ** texture, 
	ID3D11RenderTargetView ** renderTargetView, ID3D11ShaderResourceView ** shaderResourceView)
{
	D3D11_TEXTURE2D_DESC desc;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;
	desc.Format = format;
	desc.Height = height;
	desc.Width = width;
	desc.MipLevels = 1;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	HRESULT result = device->CreateTexture2D(&desc, nullptr, texture);
	if (FAILED(result))
		return result;

	result = device->CreateRenderTargetView(*texture, nullptr, renderTargetView);
	if (FAILED(result))
		return result;

	result = device->CreateShaderResourceView(*texture, nullptr, shaderResourceView);
	return result;
}

HRESULT DXTCreateRenderTargetFromBackBuffer(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11RenderTargetView** renderTargetView)
{
	ID3D11Texture2D* backBuffer;
	swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	HRESULT result = device->CreateRenderTargetView(backBuffer, nullptr, renderTargetView);
	backBuffer->Release();
	return result;
}

void DXTBytecodeBlob::Destroy()
{
	delete[] reinterpret_cast<char*>(Bytecode);
}

void DXTInputHandlerDefault::AddInputInterface(DXTInputEventInterface * obj)
{
	inputInterfaces.push_back(obj);
}

void DXTInputHandlerDefault::ClearInterfaces()
{
	inputInterfaces.clear();
}

void DXTInputHandlerDefault::OnMouseDown(const DXTMouseEventArgs & mouseEvent)
{
	for (auto obj : inputInterfaces)
		obj->OnMouseDown(mouseEvent);
}

void DXTInputHandlerDefault::OnMouseUp(const DXTMouseEventArgs & mouseEvent)
{
	for (auto obj : inputInterfaces)
		obj->OnMouseUp(mouseEvent);
}

void DXTInputHandlerDefault::OnMouseMove(const DXTMouseMoveEventArgs & mouseEvent)
{
	for (auto obj : inputInterfaces)
		obj->OnMouseMove(mouseEvent);
}

void DXTInputHandlerDefault::OnKeyDown(const DXTKeyEventArgs & keyEvent)
{
	for (auto obj : inputInterfaces)
		obj->OnKeyDown(keyEvent);
}

void DXTInputHandlerDefault::OnKeyUp(const DXTKeyEventArgs & keyEvent)
{
	for (auto obj : inputInterfaces)
		obj->OnKeyUp(keyEvent);
}

DXTWindowEventHandlerDefault::DXTWindowEventHandlerDefault() :
	swapChain(nullptr)
{
}

void DXTWindowEventHandlerDefault::SetSwapChain(IDXGISwapChain * chain)
{
	swapChain = chain;
}

void DXTWindowEventHandlerDefault::OnWindowMove()
{
	if (swapChain != nullptr)
		swapChain->Present(0, 0);
}

void DXTWindowEventHandlerDefault::OnFullscreenExit()
{
}

void DXTWindowEventHandlerDefault::OnFullscreenReenter()
{
	if (swapChain != nullptr)
		swapChain->SetFullscreenState(true, nullptr);
}

void DXTCameraBase::GetViewProjectionMatrix(DirectX::XMFLOAT4X4 * matrixOut, const DXTExtent2D& viewportSize)
{
	XMFLOAT4X4 view;
	XMFLOAT4X4 proj;
	GetViewMatrix(&view);
	GetProjectionMatrix(&proj, viewportSize);
	XMMATRIX viewMat = XMLoadFloat4x4(&view);
	XMMATRIX projMat = XMLoadFloat4x4(&proj);
	XMMATRIX resultMat = XMMatrixMultiply(viewMat, projMat);
	XMStoreFloat4x4(matrixOut, resultMat);
}
