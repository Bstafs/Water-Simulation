//--------------------------------------------------------------------------------------
// File: main.cpp
//
// This application demonstrates animation using matrix transformations
//
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729722.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#define _XM_NO_INTRINSICS_

#include "Application.h"

DirectX::XMFLOAT4 g_EyePosition(0.0f, 0, -3, 1.0f);

// Forward Declaration
// Functions
HRESULT		InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT		InitDevice();
HRESULT		InitMesh();
HRESULT		InitWorld(int width, int height, HWND hwnd);
void		CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void		Render();
void RenderToTarget();
void UpdateCamera();
void DetectInput(double deltaTime);
void IMGUI();
void SetShader(wstring filename);
void SetPPShader(wstring fn);
void UpdateFunctions();
void RenderDeferred();
void RenderDeferredShadowsDirectional();
// Globals

// DirectX Setup
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;

// Devices
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11Device1* g_pd3dDevice1 = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
ID3D11DeviceContext1* g_pImmediateContext1 = nullptr;

// Swap Chain
IDXGISwapChain* g_pSwapChain = nullptr;
IDXGISwapChain1* g_pSwapChain1 = nullptr;

// Render Targets
ID3D11RenderTargetView* g_pRenderTargetView;
ID3D11RenderTargetView* g_pRTTRenderTargetView = nullptr;
ID3D11RenderTargetView* g_pGbufferRenderTargetView[6];
ID3D11RenderTargetView* g_pGbufferRenderLightingTargetView = nullptr;

// Textures
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11Texture2D* g_pDepthStencilShadow;
ID3D11Texture2D* g_pRTTRrenderTargetTexture = nullptr;
ID3D11Texture2D* g_pGbufferTargetTextures[6];
ID3D11Texture2D* g_pGbufferTargetLightingTextures = nullptr;

// Shader Resources
ID3D11ShaderResourceView* g_pRTTShaderResourceView = nullptr;
ID3D11ShaderResourceView* g_pQuadShaderResourceView = nullptr;
ID3D11ShaderResourceView* g_pGbufferShaderResourceView[6];
ID3D11ShaderResourceView* g_pGbufferShaderResourceLightingView = nullptr;
ID3D11ShaderResourceView* g_pShadowShaderResourceView;

// Stencils
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11DepthStencilView* g_pDepthStencilShadowView;

// Shaders
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;

ID3D11VertexShader* g_pQuadVS = nullptr;
ID3D11PixelShader* g_pQuadPS = nullptr;

ID3D11VertexShader* g_pGbufferVS = nullptr;
ID3D11PixelShader* g_pGbufferPS = nullptr;

ID3D11VertexShader* g_pGbufferLightingVS = nullptr;
ID3D11PixelShader* g_pGbufferLightingPS = nullptr;

//Shadow Depth
ID3D11VertexShader* g_pShadowPreVS = nullptr;
ID3D11PixelShader* g_pShadowPrePS = nullptr;

//Shadows
ID3D11VertexShader* g_pShadowVS = nullptr;
ID3D11PixelShader* g_pShadowPS = nullptr;

// Input layouts
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11InputLayout* g_pQuadLayout = nullptr;

// Buffers
ID3D11Buffer* g_pConstantBuffer = nullptr;
ID3D11Buffer* g_pLightConstantBuffer = nullptr;
ID3D11Buffer* g_pBlurBufferVertical = nullptr;
ID3D11Buffer* g_pBlurBufferHorizontal = nullptr;
ID3D11Buffer* g_pMotionBlurBuffer = nullptr;
ID3D11Buffer* g_pQuadVB = nullptr;
ID3D11Buffer* g_pQuadIB = nullptr;

// Descriptons
D3D11_TEXTURE2D_DESC textureDesc;
D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;

// Blend States
ID3D11BlendState* blendState = nullptr;

// Structs
struct SCREEN_VERTEX
{
	XMFLOAT3 pos;
	XMFLOAT2 tex;
};

SCREEN_VERTEX svQuad[4];

// Lights
XMFLOAT4 LightPosition(g_EyePosition);
Light					g_Lighting;

// Window Size
int						g_viewWidth;
int						g_viewHeight;

//GameObjects
DrawableGameObject		g_GameObject;
Plane g_PlaneObject;

//Camera
XMMATRIX                g_View;
XMMATRIX                g_Projection;
Camera* g_pCamera0 = nullptr;
Camera* g_pCurrentCamera = nullptr;
float currentPosZ = -2.0f;
float currentPosX = 0.0f;
float currentPosY = 0.0f;
float rotationX = 0.0f;
float rotationY = 0.0f;

// ImGui
wstring shaderName(L"shader.fx");
const wchar_t* filename = shaderName.c_str();

wstring ppName(L"shaderQuad.fx");
const wchar_t* ppFileName = ppName.c_str();

LPCSTR quadString1 = "QuadVS";
LPCSTR quadString2 = "QuadVS";

bool quadTexture = false;

int deferredRenderState = 0;

unsigned int deferredID = 0;

unsigned int renderID = 0;

unsigned int ppID = 0;

unsigned int updateID = 2;

unsigned int lightTypeNumber = 0;
unsigned int lightDepthNumber = 0;

unsigned int gBufferBlend = 0;
unsigned int gBufferTextures = 0;

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			UpdateFunctions();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}

void UpdateFunctions()
{
	UpdateCamera();

	switch (updateID)
	{
	case 0:
	{
		Render();
		break;
	}
	case 1:
	{
		RenderToTarget();
		break;
	}
	case 2:
	{
		RenderDeferred();
		break;
	}
	case 3:
	{
		RenderDeferredShadowsDirectional();
		break;
	}

	}
}

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 1280, 720 };

	g_viewWidth = 1280;
	g_viewHeight = 720;

	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);
	g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 5",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

HRESULT InitDevice()
{

	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	//IDXGIFactory2* dxgiFactory2 = nullptr;
	//hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	//if (dxgiFactory2)
	//{
	//	// DirectX 11.1 or later
	//	hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
	//	if (SUCCEEDED(hr))
	//	{
	//		(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
	//	}

	//	DXGI_SWAP_CHAIN_DESC1 sd = {};
	//	sd.Width = width;
	//	sd.Height = height;
	//	sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;//  DXGI_FORMAT_R16G16B16A16_FLOAT;////DXGI_FORMAT_R8G8B8A8_UNORM;
	//	sd.SampleDesc.Count = 1;
	//	sd.SampleDesc.Quality = 0;
	//	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//	sd.BufferCount = 1;

	//	hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
	//	if (SUCCEEDED(hr))
	//	{
	//		hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
	//	}

	//	dxgiFactory2->Release();
	//}
	//else
	//{
	//	// DirectX 11.0 systems
	//	DXGI_SWAP_CHAIN_DESC sd = {};
	//	sd.BufferCount = 1;
	//	sd.BufferDesc.Width = width;
	//	sd.BufferDesc.Height = height;
	//	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//	sd.BufferDesc.RefreshRate.Numerator = 60;
	//	sd.BufferDesc.RefreshRate.Denominator = 1;
	//	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//	sd.OutputWindow = g_hWnd;
	//	sd.SampleDesc.Count = 1;
	//	sd.SampleDesc.Quality = 0;
	//	sd.Windowed = FALSE;

	//	hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	//}

	//// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	//dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

	//dxgiFactory->Release();

 //  if (FAILED(hr))
	//	return hr;

	//// Create SwapChain
	UINT maxQuality = 0;
	UINT sampleCount = 1;
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;// DXGI_FORMAT_R16G16B16A16_FLOAT;//  DXGI_FORMAT_R16G16B16A16_FLOAT;////DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.SampleDesc.Count = sampleCount;
	sd.SampleDesc.Quality = maxQuality;
	sd.OutputWindow = g_hWnd;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.Windowed = TRUE;

	//hr = g_pd3dDevice->CheckMultisampleQualityLevels(sd.BufferDesc.Format, sd.SampleDesc.Count, &maxQuality);
	//maxQuality -= 1;

	sd.SampleDesc.Quality = maxQuality;
	hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);

	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth = {};
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_R24G8_TYPELESS;
	descDepth.SampleDesc.Count = sampleCount;
	descDepth.SampleDesc.Quality = maxQuality;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;


	hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencilShadow);
	if (FAILED(hr))
		return hr;


	//Texture Render
	D3D11_TEXTURE2D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	textureDesc.SampleDesc.Count = sampleCount;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	hr = g_pd3dDevice->CreateTexture2D(&textureDesc, nullptr, &g_pRTTRrenderTargetTexture);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateTexture2D(&textureDesc, nullptr, &g_pGbufferTargetLightingTextures);
	if (FAILED(hr))
		return hr;

	for (int i = 0; i < 6; i++)
	{
		hr = g_pd3dDevice->CreateTexture2D(&textureDesc, nullptr, &g_pGbufferTargetTextures[i]);
		if (FAILED(hr))
			return hr;
	}

	renderTargetViewDesc.Format = textureDesc.Format;
	renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;

	// Create the render target view.
	hr = g_pd3dDevice->CreateRenderTargetView(g_pRTTRrenderTargetTexture, &renderTargetViewDesc, &g_pRTTRenderTargetView);
	if (FAILED(hr))
		return hr;

	// Deferred Rendering
	for (int i = 0; i < 6; i++)
	{
		hr = g_pd3dDevice->CreateRenderTargetView(g_pGbufferTargetTextures[i], &renderTargetViewDesc, &g_pGbufferRenderTargetView[i]);
		if (FAILED(hr))
			return hr;
	}


	hr = g_pd3dDevice->CreateRenderTargetView(g_pGbufferTargetLightingTextures, &renderTargetViewDesc, &g_pGbufferRenderLightingTargetView);
	if (FAILED(hr))
		return hr;

	shaderResourceViewDesc.Format = textureDesc.Format;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;

	// Create the shader resource view.
	hr = g_pd3dDevice->CreateShaderResourceView(g_pRTTRrenderTargetTexture, &shaderResourceViewDesc, &g_pRTTShaderResourceView);
	g_pRTTRrenderTargetTexture->Release();
	if (FAILED(hr))
		return hr;

	for (int i = 0; i < 6; i++)
	{
		hr = g_pd3dDevice->CreateShaderResourceView(g_pGbufferTargetTextures[i], &shaderResourceViewDesc, &g_pGbufferShaderResourceView[i]);
		g_pGbufferTargetTextures[i]->Release();
		if (FAILED(hr))
			return hr;
	}

	hr = g_pd3dDevice->CreateShaderResourceView(g_pGbufferTargetLightingTextures, &shaderResourceViewDesc, &g_pGbufferShaderResourceLightingView);
	g_pGbufferTargetLightingTextures->Release();
	if (FAILED(hr))
		return hr;
	// QUAD

// Vertices
	SimpleVertexQuad v[] =
	{

	{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },

	{ XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },

	{ XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },

	{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

	};

	// generate vb
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertexQuad) * 6;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = v;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pQuadVB);
	if (FAILED(hr))
		return hr;

	// Create index buffer
	WORD indices[] =
	{
	  0,1,2,
	  0,2,3,
	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * 6;        // 36 vertices needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pQuadIB);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencilShadow, &descDSV, &g_pDepthStencilShadowView);
	if (FAILED(hr))
		return hr;



	shaderResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;



	hr = g_pd3dDevice->CreateShaderResourceView(g_pDepthStencilShadow, &shaderResourceViewDesc, &g_pShadowShaderResourceView);
	g_pDepthStencilShadow->Release();
	if (FAILED(hr))
		return hr;



	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	//Blend state setup
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	g_pd3dDevice->CreateBlendState(&blendDesc, &blendState);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	hr = InitMesh();
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"Failed to initialise mesh.", L"Error", MB_OK);
		return hr;
	}

	hr = InitWorld(width, height, g_hWnd);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"Failed to initialise world.", L"Error", MB_OK);
		return hr;
	}

	hr = g_GameObject.initMesh(g_pd3dDevice, g_pImmediateContext);
	if (FAILED(hr))
		return hr;

	hr = g_PlaneObject.initMesh(g_pd3dDevice, g_pImmediateContext);
	if (FAILED(hr))
		return hr;

	return S_OK;
}

HRESULT		InitMesh()
{
	// Compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;
	HRESULT hr = CompileShaderFromFile(filename, "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Compile the vertex shader
	pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"shader_depth.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pShadowPreVS);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Layout
		// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the vertex shader
	pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"deffered_shader.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pGbufferVS);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"deffered_shader_shadow.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pShadowVS);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}


	// Compile the vertex shader
	pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"deffered_shader_light.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pGbufferLightingVS);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Compile the vertex shader
	pVSBlob = nullptr;
	hr = CompileShaderFromFile(ppFileName, "QuadVS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pQuadVS);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layoutQuad[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	numElements = ARRAYSIZE(layoutQuad);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layoutQuad, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pQuadLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(filename, "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader
	pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"shader_depth.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pShadowPrePS);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader
	pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"deffered_shader.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pGbufferPS);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"deffered_shader_shadow.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pShadowPS);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader
	pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"deffered_shader_light.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pGbufferLightingPS);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;


	// Compile the pixel shader
	pPSBlob = nullptr;
	hr = CompileShaderFromFile(ppFileName, "QuadPS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pQuadPS);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Create the constant buffer
	D3D11_BUFFER_DESC bd = {};
	//bd.Usage = D3D11_USAGE_DEFAULT;
	//bd.ByteWidth = sizeof(SimpleVertex) * 36;
	//bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	//bd.CPUAccessFlags = 0;
	//bd.Usage = D3D11_USAGE_DEFAULT;
	//bd.ByteWidth = sizeof(WORD) * 36;        // 36 vertices needed for 12 triangles in a triangle list
	//bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	//bd.CPUAccessFlags = 0;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);
	if (FAILED(hr))
		return hr;

	// Create the light constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(LightPropertiesConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pLightConstantBuffer);
	if (FAILED(hr))
		return hr;

	// Create the blur constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(BlurBufferHorizontal);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pBlurBufferHorizontal);
	if (FAILED(hr))
		return hr;

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(BlurBufferVertical);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pBlurBufferVertical);
	if (FAILED(hr))
		return hr;

	// Motion Blur Buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(MotionBlurBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pMotionBlurBuffer);
	if (FAILED(hr))
		return hr;

	return hr;
}

HRESULT		InitWorld(int width, int height, HWND hwnd)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplWin32_Init(g_hWnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pImmediateContext);
	ImGui::StyleColorsClassic();

	g_PlaneObject.m_positionPlane = XMFLOAT3(0.0f, -1.0f, 0.0f);

	g_Lighting.Position.x = 0.0f;
	g_Lighting.Position.y = -5.0f;
	g_Lighting.Position.z = 0.0f;


	g_Lighting.Direction.x = 0.0f;
	g_Lighting.Direction.y = -1.0f;
	g_Lighting.Direction.z = 1.0f;

	g_pCamera0 = new Camera(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), g_viewWidth, g_viewHeight, 0.01f, 100.0f);
	g_pCurrentCamera = g_pCamera0;
	g_pCurrentCamera->SetView();
	g_pCurrentCamera->SetProjection();

	//XMVECTOR LightDirection = XMVectorSet(0.0f, -1.0f, 1.0f, 0.0f);
	//LightDirection = XMVector3Normalize(LightDirection);
	//XMStoreFloat4(&g_Lighting.Direction, LightDirection);

	return S_OK;
}

void CleanupDevice()
{
	g_GameObject.cleanup();
	g_PlaneObject.cleanup();

	// Remove any bound render target or depth/stencil buffer
	ID3D11RenderTargetView* nullViews[] = { nullptr };
	g_pImmediateContext->OMSetRenderTargets(_countof(nullViews), nullViews, nullptr);

	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	// Flush the immediate context to force cleanup
	if (g_pImmediateContext1) g_pImmediateContext1->Flush();
	g_pImmediateContext->Flush();

	if (g_pLightConstantBuffer)	g_pLightConstantBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();
	if (g_pConstantBuffer) g_pConstantBuffer->Release();
	if (g_pBlurBufferVertical) g_pBlurBufferVertical->Release();
	if (g_pBlurBufferHorizontal) g_pBlurBufferHorizontal->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pRTTRenderTargetView) g_pRTTRenderTargetView->Release();
	if (g_pQuadPS) g_pQuadPS->Release();
	if (g_pQuadVS) g_pQuadVS->Release();
	if (g_pQuadLayout) g_pQuadLayout->Release();
	if (g_pGbufferPS)g_pGbufferPS->Release();
	if (g_pGbufferVS)g_pGbufferVS->Release();
	if (g_pGbufferLightingPS) g_pGbufferLightingPS->Release();
	if (g_pGbufferLightingVS) g_pGbufferLightingVS->Release();
	if (g_pSwapChain1) g_pSwapChain1->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext1) g_pImmediateContext1->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();

	ID3D11Debug* debugDevice = nullptr;
	g_pd3dDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&debugDevice));

	if (g_pd3dDevice1) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();

	// handy for finding dx memory leaks
	debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

	if (debugDevice)
		debugDevice->Release();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_LBUTTONDOWN:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		break;
	}
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// Note that this tutorial does not handle resizing (WM_SIZE) requests,
		// so we created the window without the resize border.

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

void SetupLightForRenderPoint()
{
	g_Lighting.Enabled = static_cast<int>(true);
	g_Lighting.LightType = 1;
	g_Lighting.Color = XMFLOAT4(Colors::White);
	g_Lighting.SpotAngle = XMConvertToRadians(45.0f);
	g_Lighting.ConstantAttenuation = 1.0f;
	g_Lighting.LinearAttenuation = 1;
	g_Lighting.QuadraticAttenuation = 1;

	XMFLOAT3 temp = g_pCurrentCamera->GetPosition();

	XMFLOAT4 temp4 = { temp.x, temp.y, temp.z, 0 };

	LightPropertiesConstantBuffer lightProperties;
	//lightProperties.EyePosition = g_Lighting.Position;
	lightProperties.EyePosition = temp4;
	lightProperties.Lights[0] = g_Lighting;
	g_pImmediateContext->UpdateSubresource(g_pLightConstantBuffer, 0, nullptr, &lightProperties, 0, 0);
}

void SetupLightForRenderDirectional()
{
	g_Lighting.Enabled = static_cast<int>(true);
	g_Lighting.LightType = 0;
	g_Lighting.Color = XMFLOAT4(Colors::White);
	g_Lighting.SpotAngle = XMConvertToRadians(45.0f);
	g_Lighting.ConstantAttenuation = 1.0f;
	g_Lighting.LinearAttenuation = 1;
	g_Lighting.QuadraticAttenuation = 1;

	XMFLOAT3 temp = g_pCurrentCamera->GetPosition();

	XMFLOAT4 temp4 = { temp.x, temp.y, temp.z, 0 };

	LightPropertiesConstantBuffer lightProperties;
	//lightProperties.EyePosition = g_Lighting.Position;
	lightProperties.EyePosition = temp4;
	lightProperties.Lights[0] = g_Lighting;
	g_pImmediateContext->UpdateSubresource(g_pLightConstantBuffer, 0, nullptr, &lightProperties, 0, 0);
}

void SetupLightForRenderDirectional(XMMATRIX lightSpace)
{
	g_Lighting.Enabled = static_cast<int>(true);
	g_Lighting.LightType = 0;
	g_Lighting.Color = XMFLOAT4(Colors::White);
	g_Lighting.SpotAngle = XMConvertToRadians(45.0f);
	g_Lighting.ConstantAttenuation = 1.0f;
	g_Lighting.LinearAttenuation = 1;
	g_Lighting.QuadraticAttenuation = 1;

	XMFLOAT3 temp = g_pCurrentCamera->GetPosition();

	XMFLOAT4 temp4 = { temp.x, temp.y, temp.z, 0 };

	LightPropertiesConstantBuffer lightProperties;
	//lightProperties.EyePosition = g_Lighting.Position;
	lightProperties.EyePosition = temp4;
	lightProperties.Lights[0] = g_Lighting;
	lightProperties.lightSpaceMatrices = lightSpace;
	g_pImmediateContext->UpdateSubresource(g_pLightConstantBuffer, 0, nullptr, &lightProperties, 0, 0);
}

void SetupLightForRenderSpot()
{
	g_Lighting.Enabled = static_cast<int>(true);
	g_Lighting.LightType = 2;
	g_Lighting.Color = XMFLOAT4(Colors::White);
	g_Lighting.SpotAngle = XMConvertToRadians(45.0f);
	g_Lighting.ConstantAttenuation = 0.1f;
	g_Lighting.LinearAttenuation = 0.1f;
	g_Lighting.QuadraticAttenuation = 0.1f;

	XMFLOAT3 temp = g_pCurrentCamera->GetPosition();

	XMFLOAT4 temp4 = { temp.x, temp.y, temp.z, 0 };

	LightPropertiesConstantBuffer lightProperties;
	//lightProperties.EyePosition = g_Lighting.Position;
	lightProperties.EyePosition = temp4;
	lightProperties.Lights[0] = g_Lighting;
	g_pImmediateContext->UpdateSubresource(g_pLightConstantBuffer, 0, nullptr, &lightProperties, 0, 0);
}

float CalculateDeltaTime()
{
	// Update our time
	static float deltaTime = 0.0f;
	static ULONGLONG timeStart = 0;
	ULONGLONG timeCur = GetTickCount64();
	if (timeStart == 0)
		timeStart = timeCur;
	deltaTime = (timeCur - timeStart) / 1000.0f;
	timeStart = timeCur;

	float FPS60 = 1.0f / 60.0f;
	static float cummulativeTime = 0;

	// cap the framerate at 60 fps 
	cummulativeTime += deltaTime;
	if (cummulativeTime >= FPS60) {
		cummulativeTime = cummulativeTime - FPS60;
	}
	else {
		return 0;
	}

	return deltaTime;
}

void DetectInput(double deltaTime)
{
	float mCameraSpeed = 0.000008f;

	// Forward
	if (GetAsyncKeyState('W'))
	{
		currentPosZ += mCameraSpeed * cos(rotationX);
		currentPosX += mCameraSpeed * sin(rotationX);
		currentPosY += mCameraSpeed * sin(rotationY);
	}
	// Backwards
	if (GetAsyncKeyState('S'))
	{
		currentPosZ -= mCameraSpeed * cos(rotationX);
		currentPosX -= mCameraSpeed * sin(rotationX);
		currentPosY += mCameraSpeed * sin(rotationY);
	}

	// Right
	if (GetAsyncKeyState('D'))
	{
		rotationX += mCameraSpeed;
	}
	// Left
	if (GetAsyncKeyState('A'))
	{
		rotationX -= mCameraSpeed;
	}

	// Up
	if (GetAsyncKeyState('E'))
	{
		rotationY += mCameraSpeed * cos(rotationY);
	}
	// Down
	if (GetAsyncKeyState('Q'))
	{
		rotationY -= mCameraSpeed * cos(rotationY);
	}

	if (rotationY > 1.5f)
	{
		rotationY = 1.5f;
	}
	if (rotationY < -1.5f)
	{
		rotationY = -1.5f;
	}

	return;
}

void UpdateCamera()
{
	float deltaTime = CalculateDeltaTime(); // capped at 60 fps

	DetectInput(deltaTime);

	g_pCurrentCamera->SetPosition(XMFLOAT3(currentPosX - sin(rotationX), currentPosY - sin(rotationY), currentPosZ - cos(rotationX)));
	g_pCurrentCamera->SetLookAt(XMFLOAT3(currentPosX, currentPosY, currentPosZ));
	g_pCurrentCamera->SetView();

	g_pCurrentCamera->SetView();
	g_pCurrentCamera->SetProjection();
}

void IMGUI()
{
	// IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Debug Window");

	ImGui::SetWindowSize(ImVec2(500.0f, 500.0f));

	if (ImGui::CollapsingHeader("Camera"))
	{
		ImGui::Text("Camera Position");
		ImGui::DragFloat("Camera Pos X", &currentPosX, 0.05f);
		ImGui::DragFloat("Camera Pos Y", &currentPosY, 0.05f);
		ImGui::DragFloat("Camera Pos Z", &currentPosZ, 0.05f);
		ImGui::Text("Camera Rotation");
		ImGui::DragFloat("Rotate on the X Axis", &rotationX, 0.05f);
		ImGui::DragFloat("Rotate on the Y Axis", &rotationY, 0.05f);
	}
	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::Text("Positions");
		ImGui::DragFloat("Light Position X", &g_Lighting.Position.x, 0.05f, -100.0f, 100.0f);
		ImGui::DragFloat("Light Position Y", &g_Lighting.Position.y, 0.05f, -100.0f, 100.0f);
		ImGui::DragFloat("Light Position Z", &g_Lighting.Position.z, 0.05f, -100.0f, 100.0f);

		ImGui::Text("Directions");
		ImGui::DragFloat("Light Direction X", &g_Lighting.Direction.x, 0.05f, -1.0f, 1.0f);
		ImGui::DragFloat("Light Direction Y", &g_Lighting.Direction.y, 0.05f, -1.0, 1.0f);
		ImGui::DragFloat("Light Direction Z", &g_Lighting.Direction.z, 0.05f, -1.0f, 1.0f);
		ImGui::Text("Light Types");
		if (ImGui::Button("Directional"))
		{
			lightTypeNumber = 0;
		}
		if (ImGui::Button("Point"))
		{
			lightTypeNumber = 1;
		}
		if (ImGui::Button("Spot"))
		{
			lightTypeNumber = 2;
		}
	}
	if (ImGui::CollapsingHeader("Objects"))
	{
		ImGui::Text("Spin");
		if (ImGui::Checkbox("Spinning", &g_GameObject.isSpinning));
		ImGui::Text("Positions");
		ImGui::DragFloat("Object Position X", &g_GameObject.m_position.x, 0.025f);
		ImGui::DragFloat("Object Position Y", &g_GameObject.m_position.y, 0.025f);
		ImGui::DragFloat("Object Position Z", &g_GameObject.m_position.z, 0.025f);
		ImGui::Text("Rotation");
		ImGui::DragFloat("Object Rotation X", &g_GameObject.m_rotation.x, 0.025f);
		ImGui::DragFloat("Object Rotation Y", &g_GameObject.m_rotation.y, 0.025f);
		ImGui::DragFloat("Object Rotation Z", &g_GameObject.m_rotation.z, 0.025f);
	}
	if (ImGui::CollapsingHeader("Rendering"))
	{
		if (ImGui::Button("Render Original"))
		{
			updateID = 0;
		}

		if (ImGui::Button("Normal Mapping"))
		{
			renderID = 0;
		}
		if (ImGui::Button("Simple Parallax"))
		{
			renderID = 1;
		}
		if (ImGui::Button("Steep Parallax"))
		{
			renderID = 2;
		}
		if (ImGui::Button("Relief Parallax"))
		{
			renderID = 3;
		}
		if (ImGui::Button("Occlusion Parallax"))
		{
			renderID = 4;
		}
		if (ImGui::Button("Occlusion Self Shadowing Parallax"))
		{
			renderID = 5;
		}
	}

	if (ImGui::CollapsingHeader("Post-Processing"))
	{		
		ImGui::Text("Render To Texture");
		if (ImGui::Button("Render To Texture"))
		{
			updateID = 1;
		}

		if (ImGui::Button("Original"))
		{
			ppID = 0;
		}

		if (ImGui::Button("Tint"))
		{
			ppID = 1;
		}

		if (ImGui::Button("Guassian Blur"))
		{
			ppID = 2;
		}

		if (ImGui::Button("Bloom"))
		{
			ppID = 3;
		}
	}

	if (ImGui::CollapsingHeader("Deferred Rendering"))
	{
		if (ImGui::Button("Render Deferred"))
		{
			updateID = 2;
		}
		if (ImGui::CollapsingHeader("Deferred Rendering - Mapping")) {
			if (ImGui::Button("Normal Mapping"))
			{
				deferredID = 0;
			}
			if (ImGui::Button("Simple Parallax"))
			{
				deferredID = 1;
			}
			if (ImGui::Button("Steep Parallax"))
			{
				deferredID = 2;
			}
			if (ImGui::Button("Relief Parallax"))
			{
				deferredID = 3;
			}
			if (ImGui::Button("Occlusion Parallax"))
			{
				deferredID = 4;
			}
		}
		if (ImGui::CollapsingHeader("Deferred Rendering - GBuffer")) {
			if (ImGui::Button("Render Deferred - GBuffer Blend"))
			{
				gBufferBlend = 0;
			}
			if (ImGui::Button("Render Deferred - GBuffer Seperate"))
			{
				gBufferBlend = 1;
			}
		}
		if (gBufferBlend == 1)
		{
			if (ImGui::Button("Render Deferred - GBuffer Seperate - Change Texture"))
			{
				gBufferTextures += 1;
			}
		}

		if (ImGui::CollapsingHeader("Deferred Rendering - Shadow Mapping")) {
			if (ImGui::Button("Render Deferred Shadow Mapping"))
			{
				updateID = 3;
			}
			if (ImGui::Button("Shadow Mapping - Non-Linear"))
			{
				lightDepthNumber = 1;
			}
			if (ImGui::Button("Shadow Mapping - Linear"))
			{
				lightDepthNumber = 0;
			}
		}

	}

	ImGui::End();

	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Render()
{
	float t = CalculateDeltaTime();
	if (t == 0.0f)
		return;

	if (renderID > 5)
	{
		renderID = 0;
	}

	// Clear the back buffer
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);

	// Clear the depth buffer to 1.0 (max depth)
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	g_GameObject.update(t, g_pImmediateContext);

	switch (lightTypeNumber)
	{
	case 0:
	{
		SetupLightForRenderDirectional();
		break;
	}
	case 1:
	{
		SetupLightForRenderPoint();
		break;
	}
	case 2:
	{
		SetupLightForRenderSpot();
		break;
	}
	}

	// get the game object world transform
	XMMATRIX mGO = XMLoadFloat4x4(g_GameObject.getTransform());
	XMMATRIX view = XMLoadFloat4x4(g_pCurrentCamera->GetView());
	XMMATRIX projection = XMLoadFloat4x4(g_pCurrentCamera->GetProjection());

	// store this and the view / projection in a constant buffer for the vertex shader to use
	ConstantBuffer cb1;
	cb1.mWorld = XMMatrixTranspose(mGO);
	cb1.mView = XMMatrixTranspose(view);
	cb1.mProjection = XMMatrixTranspose(projection);
	cb1.vOutputColor = XMFLOAT4(0, 0, 0, 0);
	cb1.renID = renderID;
	//cb1.defID = deferredID;
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb1, 0, 0);

	// Render the cube
	//Vertex Shader
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_GameObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_GameObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	//Pixel shader
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

	ID3D11Buffer* materialCB = g_GameObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShaderResources(0, 1, g_GameObject.getTextureResourceView());
	g_GameObject.draw(g_pImmediateContext);

	IMGUI();

	// Present our back buffer to our front buffer
	g_pSwapChain->Present(0, 0);
}

void RenderToTarget()
{
	float t = CalculateDeltaTime();
	if (t == 0.0f)
		return;

	// First Render

	// Clear the back buffer
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);
	g_pImmediateContext->ClearRenderTargetView(g_pRTTRenderTargetView, Colors::Green);

	// Clear the depth buffer to 1.0 (max depth)
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	//Set Render Target
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRTTRenderTargetView, g_pDepthStencilView);

	// Update GameObject
	g_GameObject.update(t, g_pImmediateContext);

	switch (lightTypeNumber)
	{
	case 0:
	{
		SetupLightForRenderDirectional();
		break;
	}
	case 1:
	{
		SetupLightForRenderPoint();
		break;
	}
	case 2:
	{
		SetupLightForRenderSpot();
		break;
	}
	}

	// GameObjects Position
	XMMATRIX mGO = XMLoadFloat4x4(g_GameObject.getTransform());
	XMMATRIX view = XMLoadFloat4x4(g_pCurrentCamera->GetView());
	XMMATRIX projection = XMLoadFloat4x4(g_pCurrentCamera->GetProjection());

	// Constant Buffer
	ConstantBuffer cb1;
	cb1.mWorld = XMMatrixTranspose(mGO);
	cb1.mView = XMMatrixTranspose(view);
	cb1.mProjection = XMMatrixTranspose(projection);
	cb1.vOutputColor = XMFLOAT4(0, 0, 0, 0);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb1, 0, 0);

	BlurBufferHorizontal cbh;
	cbh.horizontal = true;
	cbh.ppID = ppID;
	cbh.padding01 = XMFLOAT2(1, 1);
	g_pImmediateContext->UpdateSubresource(g_pBlurBufferHorizontal, 0, nullptr, &cbh, 0, 0);

	BlurBufferVertical cbv;
	cbv.vertical = true;
	cbv.padding02 = XMFLOAT3(1, 1, 1);
	g_pImmediateContext->UpdateSubresource(g_pBlurBufferVertical, 0, nullptr, &cbv, 0, 0);

	MotionBlurBuffer cbm;;
	XMMATRIX inverseProj = XMMatrixInverse(nullptr, projection);
	cbm.mWorld = XMMatrixTranspose(mGO);
	cbm.mView = XMMatrixTranspose(view);
	cbm.mProjection = XMMatrixTranspose(projection);
	cbm.mInverseProjection = XMMatrixTranspose(inverseProj);
	cbm.vOutputColor = cb1.vOutputColor = XMFLOAT4(0, 0, 0, 0);
	g_pImmediateContext->UpdateSubresource(g_pMotionBlurBuffer, 0, nullptr, &cbv, 0, 0);

	// Vertex Shader Layout
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_GameObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_GameObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	// Vertex Shader
	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	// Vertex Constant Buffer
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	// Vertex Light Buffer
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	// Pixel shader
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
	// Pixel Sampler
	g_pImmediateContext->PSSetSamplers(1, 2, g_GameObject.getTextureSamplerState());
	// Material Pixel Buffer
	ID3D11Buffer* materialCB = g_GameObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	// Light Pixel Buffer
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);
	// Constant Pixel Buffer
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	// Shader Pixel Resource
	g_pImmediateContext->PSSetShaderResources(0, 1, g_GameObject.getTextureResourceView());

	// Draw
	g_GameObject.draw(g_pImmediateContext);

	// Second Render
	// Set Render Target
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// Clear the depth buffer 
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Post Process Buffer

	// Set VB, IB and layout for Quad
	stride = sizeof(SimpleVertexQuad);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pQuadVB, &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_pQuadIB, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pQuadLayout);

	//Vertex Shader Quad
	g_pImmediateContext->VSSetShader(g_pQuadVS, nullptr, 0);

	// Pixel shader Quad
	g_pImmediateContext->PSSetShader(g_pQuadPS, nullptr, 0);

	// Pixel Sampler 
	g_pImmediateContext->PSSetSamplers(0, 2, g_GameObject.getTextureSamplerState());

	// Pixel Blur Buffer

	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pBlurBufferHorizontal);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &g_pBlurBufferVertical);

	// Pixel Shader Resource
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_pRTTShaderResourceView);

	// Draw Quad
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	// ImGui
	IMGUI();

	cbm.mPreviousProjection = XMMatrixTranspose(projection);
	g_pImmediateContext->UpdateSubresource(g_pMotionBlurBuffer, 0, nullptr, &cbv, 0, 0);

	// Present our back buffer to our front buffer
	g_pSwapChain->Present(0, 0);

	// Set Shader Resource to Null / Clear
	ID3D11ShaderResourceView* const shaderClear[1] = { NULL };
	g_pImmediateContext->PSSetShaderResources(0, 1, shaderClear);
}

void RenderDeferred()
{
	float t = CalculateDeltaTime();
	if (t == 0.0f)
		return;

	if (deferredID > 4)
	{
		deferredID = 0;
	}

	if (gBufferTextures > 6)
	{
		gBufferTextures = 0;
	}

	if (gBufferBlend == 0)
	{
		g_Lighting.gBufferTextures = 0;
	}
	else if(gBufferBlend == 1)
	{
		g_Lighting.gBufferTextures = 1;
	}

	// get the game object world transform
	XMMATRIX mGOCube = XMLoadFloat4x4(g_GameObject.getTransform());
	XMMATRIX mGOPlane = XMLoadFloat4x4(g_PlaneObject.getTransform());
	XMMATRIX view = XMLoadFloat4x4(g_pCurrentCamera->GetView());
	XMMATRIX projection = XMLoadFloat4x4(g_pCurrentCamera->GetProjection());

	ConstantBuffer cb;

	// Clear the back buffer 
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[0], Colors::Black); // Normal
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[1], Colors::Black); // Diffuse
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[2], Colors::Black); // Specular
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[3], Colors::Black); // Position
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[4], Colors::Black); // Ambient
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[5], Colors::White); // Emissive
	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderLightingTargetView, Colors::Black);
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::Black);
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	g_GameObject.update(t, g_pImmediateContext);
	g_PlaneObject.update(t, g_pImmediateContext);

	switch (lightTypeNumber)
	{
	case 0:
	{
		SetupLightForRenderDirectional();
		break;
	}
	case 1:
	{
		SetupLightForRenderPoint();
		break;
	}
	case 2:
	{
		SetupLightForRenderSpot();
		break;
	}
	}

	// G Buffer Pass
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(6, &g_pGbufferRenderTargetView[0], g_pDepthStencilView);

	mGOCube = XMLoadFloat4x4(g_GameObject.getTransform());
	mGOPlane = XMLoadFloat4x4(g_PlaneObject.getTransform());
	view = XMLoadFloat4x4(g_pCurrentCamera->GetView());
	projection = XMLoadFloat4x4(g_pCurrentCamera->GetProjection());

	cb.mWorld = XMMatrixTranspose(mGOPlane);
	cb.mView = XMMatrixTranspose(view);
	cb.mProjection = XMMatrixTranspose(projection);
	cb.vOutputColor = XMFLOAT4(0, 0, 0, 0);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Plane Render
	UINT stride = sizeof(SimpleVertexPlane);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_PlaneObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_PlaneObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	g_pImmediateContext->VSSetShader(g_pGbufferVS, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShader(g_pGbufferPS, nullptr, 0);
	ID3D11Buffer* materialCB = g_PlaneObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);
	g_pImmediateContext->PSSetSamplers(0, 1, g_PlaneObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 1, g_PlaneObject.getTextureResourceView());

	g_PlaneObject.draw(g_pImmediateContext);

	// Cube Render
	cb.mWorld = XMMatrixTranspose(mGOCube);
	cb.defID = deferredID;
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	stride = sizeof(SimpleVertex);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_GameObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_GameObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	g_pImmediateContext->VSSetShader(g_pGbufferVS, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShader(g_pGbufferPS, nullptr, 0);
	materialCB = g_GameObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetSamplers(0, 1, g_GameObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 1, g_GameObject.getTextureResourceView());

	g_GameObject.draw(g_pImmediateContext);

	// Lighting Pass
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pGbufferRenderLightingTargetView, g_pDepthStencilView);

	float blend[4] = { 1,1,1, 1 };
	g_pImmediateContext->OMSetBlendState(blendState, blend, 0xFFFFFFFF);

	stride = sizeof(SimpleVertexQuad);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pQuadVB, &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_pQuadIB, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pQuadLayout);

	g_pImmediateContext->PSSetShader(g_pGbufferLightingPS, nullptr, 0);
	g_pImmediateContext->VSSetShader(g_pGbufferLightingVS, nullptr, 0);
	g_pImmediateContext->PSSetShaderResources(0, 6, &g_pGbufferShaderResourceView[0]);
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	// Quad Pass
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	g_pImmediateContext->VSSetShader(g_pQuadVS, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pQuadPS, nullptr, 0);

	if (gBufferBlend == 0)
	{
		g_pImmediateContext->PSSetShaderResources(0, 1, &g_pGbufferShaderResourceLightingView);
	}
	else if (gBufferBlend == 1)
	{
		g_pImmediateContext->PSSetShaderResources(0, 1, &g_pGbufferShaderResourceView[gBufferTextures]);
	}
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	IMGUI();

	g_pImmediateContext->OMSetBlendState(NULL, blend, 0xFFFFFFFF);
	g_pImmediateContext->OMSetDepthStencilState(NULL, 0);
	// Present our back buffer to our front buffer
	g_pSwapChain->Present(0, 0);

	// Set Shader Resource to Null / Clear
	ID3D11ShaderResourceView* const shaderClear[1] = { NULL };
	for (int i = 0; i < 30; i++)
	{
		g_pImmediateContext->PSSetShaderResources(i, 1, shaderClear);
	}
}

void RenderDeferredShadowsDirectional()
{
	float t = CalculateDeltaTime();
	if (t == 0.0f)
		return;

	if (deferredID > 4)
	{
		deferredID = 0;
	}

	if (lightDepthNumber == 0)
	{
		g_Lighting.LinearDepth = 0;
	}
	else if (lightDepthNumber == 1)
	{
		g_Lighting.LinearDepth = 1;
	}

	XMMATRIX mGOCube = XMLoadFloat4x4(g_GameObject.getTransform());
	XMMATRIX mGOPlane = XMLoadFloat4x4(g_PlaneObject.getTransform());
	XMMATRIX shadowCube;
	// Clear the back buffer 
	for (int i = 0; i < 6; i++)
	{
		g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderTargetView[i], Colors::Black);
	}

	g_pImmediateContext->ClearRenderTargetView(g_pGbufferRenderLightingTargetView, Colors::Black);
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::Black);
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilShadowView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	g_GameObject.update(t, g_pImmediateContext);
	g_PlaneObject.update(t, g_pImmediateContext);

	// Shadow Buffer Pass

	LightPropertiesConstantBuffer lightProperties;
	lightProperties.EyePosition = g_Lighting.Position;
	lightProperties.Lights[0] = g_Lighting;
	g_pImmediateContext->UpdateSubresource(g_pLightConstantBuffer, 0, nullptr, &lightProperties, 0, 0);

	XMFLOAT4X4 lightViewMatrix;
	XMFLOAT4 EyePosition = { g_Lighting.Position.x, g_Lighting.Position.y, g_Lighting.Position.z, 0.0f };
	XMFLOAT4 EyeDirection = { g_Lighting.Direction.x, g_Lighting.Direction.y, g_Lighting.Direction.z, 0.0f };
	XMFLOAT4 UpDirection = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&lightViewMatrix, XMMatrixLookAtLH(XMLoadFloat4(&EyePosition), XMLoadFloat4(&EyeDirection), XMLoadFloat4(&UpDirection)));

	XMMATRIX Proj = XMLoadFloat4x4(g_pCurrentCamera->GetProjection());
	XMMATRIX ProjOrtho = XMMatrixOrthographicLH(1280.0f, 720.0f, 0.01f, 100.0f);

	shadowCube = XMMatrixMultiply(XMMatrixTranspose(ProjOrtho), XMMatrixTranspose(XMLoadFloat4x4(&lightViewMatrix)));

	switch (lightTypeNumber)
	{
	case 0:
	{
		SetupLightForRenderDirectional(shadowCube);
		break;
	}
	case 1:
	{
		SetupLightForRenderPoint();
		break;
	}
	case 2:
	{
		SetupLightForRenderSpot();
		break;
	}
	}

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilShadowView);

	ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose(mGOPlane);
	cb.mView = XMMatrixTranspose(XMLoadFloat4x4(&lightViewMatrix));
	cb.mProjection = XMMatrixTranspose(ProjOrtho);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Plane Render
	UINT stride = sizeof(SimpleVertexPlane);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_PlaneObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_PlaneObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	g_pImmediateContext->VSSetShader(g_pShadowPreVS, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShader(g_pShadowPrePS, nullptr, 0);
	ID3D11Buffer* materialCB = g_PlaneObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);
	g_pImmediateContext->PSSetSamplers(0, 1, g_PlaneObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 1, g_PlaneObject.getTextureResourceView());

	g_PlaneObject.draw(g_pImmediateContext);

	// Cube Render
	cb.mWorld = XMMatrixTranspose(mGOCube);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	stride = sizeof(SimpleVertex);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_GameObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_GameObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	materialCB = g_GameObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetSamplers(0, 1, g_GameObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 1, g_GameObject.getTextureResourceView());

	g_GameObject.draw(g_pImmediateContext);

	// G Buffer Pass
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(6, &g_pGbufferRenderTargetView[0], g_pDepthStencilView);

	cb.mWorld = XMMatrixTranspose(mGOPlane);
	cb.mView = XMMatrixTranspose(XMLoadFloat4x4(g_pCurrentCamera->GetView()));
	cb.mProjection = XMMatrixTranspose(XMLoadFloat4x4(g_pCurrentCamera->GetProjection()));
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	// Plane Render
	stride = sizeof(SimpleVertexPlane);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_PlaneObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_PlaneObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	g_pImmediateContext->VSSetShader(g_pGbufferVS, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShader(g_pGbufferPS, nullptr, 0);
	materialCB = g_PlaneObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);
	g_pImmediateContext->PSSetSamplers(0, 1, g_PlaneObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 1, g_PlaneObject.getTextureResourceView());

	g_PlaneObject.draw(g_pImmediateContext);

	// Cube Render
	cb.mWorld = XMMatrixTranspose(mGOCube);
	cb.defID = deferredID;
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	stride = sizeof(SimpleVertex);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, g_GameObject.getVertexBuffer(true), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_GameObject.getIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	g_pImmediateContext->VSSetShader(g_pGbufferVS, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShader(g_pGbufferPS, nullptr, 0);
	materialCB = g_GameObject.getMaterialConstantBuffer();
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(1, 1, &materialCB);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetSamplers(0, 1, g_GameObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 1, g_GameObject.getTextureResourceView());

	g_GameObject.draw(g_pImmediateContext);

	// Lighting Pass
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pGbufferRenderLightingTargetView, g_pDepthStencilView);

	float blend[4] = { 1,1,1, 1 };
	g_pImmediateContext->OMSetBlendState(blendState, blend, 0xFFFFFFFF);

	stride = sizeof(SimpleVertexQuad);
	offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pQuadVB, &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_pQuadIB, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetInputLayout(g_pQuadLayout);

	g_pImmediateContext->VSSetShader(g_pShadowVS, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);

	g_pImmediateContext->PSSetShader(g_pShadowPS, nullptr, 0);
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pLightConstantBuffer);
	g_pImmediateContext->PSSetSamplers(0, 1, g_PlaneObject.getTextureSamplerState());
	g_pImmediateContext->PSSetSamplers(0, 1, g_GameObject.getTextureSamplerState());
	g_pImmediateContext->PSSetShaderResources(0, 6, &g_pGbufferShaderResourceView[0]);
	g_pImmediateContext->PSSetShaderResources(6, 1, &g_pShadowShaderResourceView);
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	// Quad Pass
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::Black);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	g_pImmediateContext->VSSetShader(g_pQuadVS, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pQuadPS, nullptr, 0);

	//g_pImmediateContext->PSSetShaderResources(0, 1, &g_pGbufferShaderResourceView[0]);
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_pGbufferShaderResourceLightingView);
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	IMGUI();

	g_pImmediateContext->OMSetBlendState(NULL, blend, 0xFFFFFFFF);
	g_pImmediateContext->OMSetDepthStencilState(NULL, 0);
	// Present our back buffer to our front buffer
	g_pSwapChain->Present(0, 0);

	// Set Shader Resource to Null / Clear
	ID3D11ShaderResourceView* const shaderClear[1] = { NULL };
	for (int i = 0; i < 30; i++)
	{
		g_pImmediateContext->PSSetShaderResources(i, 1, shaderClear);
	}
}