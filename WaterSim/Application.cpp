#include "Application.h"

#include <Effects.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Note: if NuGet Package not found go to tools->options->NuGet Package Manager
// Package Sources -> Add Source -> name = nuget.org, source = https://api.nuget.org/v3/index.json
// Click Update then Ok.

// Right click solution -> Restore NuGet Packages.


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
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

bool Application::HandleKeyboard()
{
	float mCameraSpeed = 0.08f;
	float mTurnCameraSpeed = 0.008f;

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
		rotationX += mTurnCameraSpeed;
	}
	// Left
	if (GetAsyncKeyState('A'))
	{
		rotationX -= mTurnCameraSpeed;
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

	return false;
}

Application::Application()
{
	_hInst = nullptr;
	_hWnd = nullptr;
	_driverType = D3D_DRIVER_TYPE_NULL;
	_featureLevel = D3D_FEATURE_LEVEL_11_0;
	_pd3dDevice = nullptr;
	_pImmediateContext = nullptr;
	_pSwapChain = nullptr;
	_pRenderTargetView = nullptr;
	_pVertexShader = nullptr;
	_pPixelShader = nullptr;
	_pVertexLayout = nullptr;
	_pVertexBuffer = nullptr;
	_pIndexBuffer = nullptr;
	_pConstantBuffer = nullptr;
	CCWcullMode = nullptr;
	CWcullMode = nullptr;
	DSLessEqual = nullptr;
	RSCullNone = nullptr;
	_WindowHeight = 0;
	_WindowWidth = 0;
}

Application::~Application()
{
	Cleanup();
}

HRESULT Application::Initialise(HINSTANCE hInstance, int nCmdShow)
{
	if (FAILED(InitWindow(hInstance, nCmdShow)))
	{
		return E_FAIL;
	}

	RECT rc;
	GetClientRect(_hWnd, &rc);
	_WindowWidth = rc.right - rc.left;
	_WindowHeight = rc.bottom - rc.top;

	if (FAILED(InitDevice()))
	{
		Cleanup();

		return E_FAIL;
	}

	// Setup Imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(_hWnd);
	ImGui_ImplDX11_Init(_pd3dDevice, _pImmediateContext);
	ImGui::StyleColorsDark();

	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\stone.dds", nullptr, &_pTextureRV);
	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\floor.dds", nullptr, &_pGroundTextureRV);
	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\waterReflection.dds", nullptr, &pReflectionTextureSRV);
	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\waterReflection.dds", nullptr, &pRefractionTextureSRV);
	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\waterReflection.dds", nullptr, &pSkyBoxTextureSRV);


	// Setup Camera
	XMFLOAT3 eye = XMFLOAT3(0.0f, 2.0f, -1.0f);
	XMFLOAT3 at = XMFLOAT3(0.0f, 2.0f, 0.0f);
	XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	_camera = new Camera(eye, at, up, (float)_renderWidth, (float)_renderHeight, 0.01f, 10000.0f);

	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(_pImmediateContext);

	// Setup the scene's light
	basicLight.AmbientLight = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	basicLight.DiffuseLight = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	basicLight.SpecularLight = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	basicLight.SpecularPower = 20.0f;
	basicLight.LightVecW = XMFLOAT3(0.0f, 1.0f, -1.0f);

	Geometry cubeGeometry;
	cubeGeometry.indexBuffer = _pIndexBuffer;
	cubeGeometry.vertexBuffer = _pVertexBuffer;
	cubeGeometry.numberOfIndices = 36;
	cubeGeometry.vertexBufferOffset = 0;
	cubeGeometry.vertexBufferStride = sizeof(SimpleVertex);

	Geometry planeGeometry;
	planeGeometry.indexBuffer = _pPlaneIndexBuffer;
	planeGeometry.vertexBuffer = _pPlaneVertexBuffer;
	planeGeometry.numberOfIndices = 6;
	planeGeometry.vertexBufferOffset = 0;
	planeGeometry.vertexBufferStride = sizeof(SimpleVertex);

	Material shinyMaterial;
	shinyMaterial.ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	shinyMaterial.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	shinyMaterial.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	shinyMaterial.specularPower = 10.0f;

	Material noSpecMaterial;
	noSpecMaterial.ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	noSpecMaterial.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	noSpecMaterial.specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	noSpecMaterial.specularPower = 0.0f;

	GameObject* gameObject = new GameObject("Floor", planeGeometry, noSpecMaterial);
	gameObject->GetTransform()->SetPosition(0.0f, 0.0f, 0.0f);
	gameObject->GetTransform()->SetScale(15.0f, 15.0f, 15.0f);
	gameObject->GetTransform()->SetRotation(XMConvertToRadians(90.0f), 0.0f, 0.0f);
	gameObject->GetAppearance()->SetTextureRV(_pGroundTextureRV);
	m_gameObjects.push_back(gameObject);

	for (int i = 0; i < NUMBER_OF_CUBES; i++)
	{
		gameObject = new GameObject("Cube " + i, cubeGeometry, shinyMaterial);
		gameObject->GetTransform()->SetScale(0.5f, 0.5f, 0.5f);
		gameObject->GetTransform()->SetPosition(0.0f, 0.5f, 0.0);
		gameObject->GetAppearance()->SetTextureRV(_pTextureRV);
		m_gameObjects.push_back(gameObject);
	}


	numbParticles = 8192;			

	mass = 0.002f;
	density = 997.0f;
	viscosity = 1.5f;
	h = 20.0f;
	g = -9.807f;
	elastisicty = 0.1f;
	numberOfParticlesDrawn = numbParticles;
	sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);

	return S_OK;
}

HRESULT Application::InitShadersAndInputLayout()
{
	HRESULT hr;

	_pVertexShader = CreateVertexShader(L"shader.fx", "VS", "vs_4_0", _pd3dDevice);
	_pPixelShader = CreatePixelShader(L"shader.fx", "PS", "ps_4_0", _pd3dDevice);

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	_pVertexLayout = CreateInputLayout(layout, numElements, _pd3dDevice);

	// Set the input layout
	_pImmediateContext->IASetInputLayout(_pVertexLayout);


	pWaterVertexShader = CreateVertexShader(L"WaterShader.fx", "VS", "vs_4_0", _pd3dDevice);
	pWaterPixelShader = CreatePixelShader(L"WaterShader.fx", "PS", "ps_4_0", _pd3dDevice);

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC waterLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElementsWater = ARRAYSIZE(waterLayout);

	pWaterInputLayout = CreateInputLayout(waterLayout, numElementsWater, _pd3dDevice);


	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = _pd3dDevice->CreateSamplerState(&sampDesc, &_pSamplerLinear);

	return hr;
}

HRESULT Application::InitVertexBuffer()
{
	HRESULT hr;

	// Create vertex buffer
	SimpleVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },

		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 24;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;

	hr = _pd3dDevice->CreateBuffer(&bd, &InitData, &_pVertexBuffer);

	if (FAILED(hr))
		return hr;

	// Create vertex buffer
	SimpleVertex planeVertices[] =
	{
		{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 5.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(5.0f, 5.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(5.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
	};

	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = planeVertices;

	hr = _pd3dDevice->CreateBuffer(&bd, &InitData, &_pPlaneVertexBuffer);

	if (FAILED(hr))
		return hr;

	SimpleVertex WaterVertices[] =
	{
	{ XMFLOAT3(-1.5f, 0.0f, -1.5), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
	{ XMFLOAT3(-1.5f, 0.0f,  1.5), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
	{ XMFLOAT3(1.5f, 0.0f,  1.5), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
	{ XMFLOAT3(1.5f, 0.0f, -1.5), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }
	};


	// Create the vertex buffer
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(SimpleVertex) * 4;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vertexData;
	ZeroMemory(&vertexData, sizeof(vertexData));
	vertexData.pSysMem = WaterVertices;
	hr = _pd3dDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &_pWaterVertexBuffer);


	return S_OK;
}

HRESULT Application::InitIndexBuffer()
{
	HRESULT hr;

	// Create index buffer
	WORD indices[] =
	{
		3, 1, 0,
		2, 1, 3,

		6, 4, 5,
		7, 4, 6,

		11, 9, 8,
		10, 9, 11,

		14, 12, 13,
		15, 12, 14,

		19, 17, 16,
		18, 17, 19,

		22, 20, 21,
		23, 20, 22
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * 36;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = indices;
	hr = _pd3dDevice->CreateBuffer(&bd, &InitData, &_pIndexBuffer);

	if (FAILED(hr))
		return hr;

	// Create plane index buffer
	WORD planeIndices[] =
	{
		0, 3, 1,
		3, 2, 1,
	};

	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * 6;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = planeIndices;
	hr = _pd3dDevice->CreateBuffer(&bd, &InitData, &_pPlaneIndexBuffer);

	if (FAILED(hr))
		return hr;

	WORD waterIndices[] =
	{
	0, 1, 2,
	0, 2, 3
	};

	// Create the index buffer
	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(WORD) * 6;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA indexData;
	ZeroMemory(&indexData, sizeof(indexData));
	indexData.pSysMem = waterIndices;
	hr = _pd3dDevice->CreateBuffer(&indexBufferDesc, &indexData, &_pWaterIndexBuffer);

	return S_OK;
}

HRESULT Application::InitWindow(HINSTANCE hInstance, int nCmdShow)
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
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"WaterSim";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	_hInst = hInstance;
	RECT rc = { 0, 0, _renderWidth, _renderHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	_hWnd = CreateWindow(L"WaterSim", L"Water Simulation Marching Cubes & SPH", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!_hWnd)
		return E_FAIL;

	ShowWindow(_hWnd, nCmdShow);

	return S_OK;
}


HRESULT Application::InitDevice()
{
	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;

	// comment this out
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
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	UINT sampleCount = 4;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = _renderWidth;
	sd.BufferDesc.Height = _renderHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = _hWnd;
	sd.SampleDesc.Count = sampleCount;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(nullptr, _driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &_pSwapChain, &_pd3dDevice, &_featureLevel, &_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
	}

	if (FAILED(hr))
		return hr;

	if (_pd3dDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
	{
		D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts = { 0 };
		(void)_pd3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
		if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
		{
			_pd3dDevice->Release();
			printf("Direct Compute is Not Supported");
			return -1;
		}
	}

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = _pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	if (FAILED(hr))
		return hr;

	hr = _pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &_pRenderTargetView);
	pBackBuffer->Release();

	if (FAILED(hr))
		return hr;

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)_renderWidth;
	vp.Height = (FLOAT)_renderHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	_pImmediateContext->RSSetViewports(1, &vp);

	InitShadersAndInputLayout();

	InitVertexBuffer();
	InitIndexBuffer();

	// Set primitive topology
	_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the constant buffer
	_pConstantBuffer = CreateConstantBuffer(sizeof(ConstantBuffer), _pd3dDevice, false);
	pWaterConstantBuffer = CreateConstantBuffer(sizeof(WaterBuffer), _pd3dDevice, false);

	// Depth Stencil Stuff
	D3D11_TEXTURE2D_DESC depthStencilDesc;

	depthStencilDesc.Width = _renderWidth;
	depthStencilDesc.Height = _renderHeight;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = sampleCount;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	_pd3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &_depthStencilBuffer);
	_pd3dDevice->CreateDepthStencilView(_depthStencilBuffer, nullptr, &_depthStencilView);

	_pImmediateContext->OMSetRenderTargets(1, &_pRenderTargetView, _depthStencilView);

	// Rasterizer
	D3D11_RASTERIZER_DESC cmdesc;

	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_NONE;
	hr = _pd3dDevice->CreateRasterizerState(&cmdesc, &RSCullNone);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = true;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	_pd3dDevice->CreateDepthStencilState(&dssDesc, &DSLessEqual);

	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));

	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_BACK;

	cmdesc.FrontCounterClockwise = true;
	hr = _pd3dDevice->CreateRasterizerState(&cmdesc, &CCWcullMode);

	cmdesc.FrontCounterClockwise = false;
	hr = _pd3dDevice->CreateRasterizerState(&cmdesc, &CWcullMode);

	D3D11_BLEND_DESC blendStateDesc;
	ZeroMemory(&blendStateDesc, sizeof(blendStateDesc));
	blendStateDesc.AlphaToCoverageEnable = false;
	blendStateDesc.RenderTarget[0].BlendEnable = true;
	blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = _pd3dDevice->CreateBlendState(&blendStateDesc, &waterBlendState);

	return hr;
}

void Application::Cleanup()
{
	if (_pImmediateContext) _pImmediateContext->ClearState();
	if (_pSamplerLinear) _pSamplerLinear->Release();

	if (_pTextureRV) _pTextureRV->Release();

	if (_pGroundTextureRV) _pGroundTextureRV->Release();

	if (_pConstantBuffer) _pConstantBuffer->Release();

	if (_pVertexBuffer) _pVertexBuffer->Release();
	if (_pIndexBuffer) _pIndexBuffer->Release();
	if (_pPlaneVertexBuffer) _pPlaneVertexBuffer->Release();
	if (_pPlaneIndexBuffer) _pPlaneIndexBuffer->Release();

	if (_pVertexLayout) _pVertexLayout->Release();
	if (_pVertexShader) _pVertexShader->Release();
	if (_pPixelShader) _pPixelShader->Release();

	if (_pRenderTargetView) _pRenderTargetView->Release();
	if (_pSwapChain) _pSwapChain->Release();
	if (_pImmediateContext) _pImmediateContext->Release();
	if (_pd3dDevice) _pd3dDevice->Release();
	if (_depthStencilView) _depthStencilView->Release();
	if (_depthStencilBuffer) _depthStencilBuffer->Release();

	if (DSLessEqual) DSLessEqual->Release();
	if (RSCullNone) RSCullNone->Release();

	if (CCWcullMode) CCWcullMode->Release();
	if (CWcullMode) CWcullMode->Release();

	if (_camera)
	{
		delete _camera;
		_camera = nullptr;
	}

	for (auto gameObject : m_gameObjects)
	{
		if (gameObject)
		{
			delete gameObject;
			gameObject = nullptr;
		}
	}

	delete sph;
	sph = nullptr;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

float Application::CalculateDeltaTime60FPS()
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
	if (cummulativeTime >= FPS60)
	{
		cummulativeTime = cummulativeTime - FPS60;
	}
	else
	{
		return 0;
	}

	return FPS60;
}

void Application::Update()
{
	float deltaTime = CalculateDeltaTime60FPS();
	if (deltaTime == 0)
	{
		return;
	}

	// Update camera

	_camera->SetPosition(XMFLOAT3(currentPosX - sin(rotationX), currentPosY - sin(rotationY), currentPosZ - cos(rotationX)));
	_camera->SetLookAt(XMFLOAT3(currentPosX, currentPosY, currentPosZ));

	_camera->Update();

	sph->deltaTimeValue = deltaTime;

	sph->Update();

	for (auto gameObject : m_gameObjects)
	{
		gameObject->Update(deltaTime);
	}

	XMMATRIX object;
	object = (XMMatrixRotationX(waterRot.x) * XMMatrixRotationY(waterRot.y) * XMMatrixRotationZ(waterRot.z)) * XMMatrixTranslation(waterPos.x, waterPos.y, waterPos.z);
	XMStoreFloat4x4(&m_myWater, object);
}

void Application::ImGui()
{

	// IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();


	ImGui::NewFrame();

	//ImGui::ShowDemoWindow();

	ImGui::Begin("Debug Window");

	ImGui::SetWindowSize(ImVec2(500.0f, 500.0f));

	ImGuiIO& io = ImGui::GetIO();

	ImGui::Text("Frames Per Second: %.1f MS / %.3f FPS", 1000.0f / io.Framerate, io.Framerate);

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
	if (ImGui::CollapsingHeader("SPH"))
	{
		int particleSize = sph->particleList.size();

		ImGui::Text("Visualize");
		ImGui::Checkbox("Visualize", &isParticleVisible);

		ImGui::InputInt("Cubes Drawn: ", &numberOfParticlesDrawn);

		ImGui::Text("Initial Values");


		if(ImGui::Button("8,192 Particles"))
		{
			numbParticles = 8196;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("16,384 Particles"))
		{
			numbParticles = 16384;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("32,768 Particles"))
		{
			numbParticles = 32768;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("65,536 Particles"))
		{
			numbParticles = 65536;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("131,072 Particles"))
		{
			numbParticles = 131072;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("262,144 Particles"))
		{
			numbParticles = 262144;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("524,288 Particles"))
		{
			numbParticles = 524288;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}
		if (ImGui::Button("1,048,576 Particles"))
		{
			numbParticles = 1048576;
			numberOfParticlesDrawn = numbParticles;
			sph = new SPH(numbParticles, mass, density, viscosity, h, g, elastisicty, 200.0f, _pImmediateContext, _pd3dDevice);
		}

		ImGui::DragFloat("Density", &sph->sphDensity, 1.0f, 0);
		ImGui::DragFloat("Smoothing Length", &sph->sphH, 0.001f, 0, 1.0f);
		ImGui::DragFloat("Gravity", &sph->sphG);

		if (ImGui::CollapsingHeader("Particle List"))
		{
			if (ImGui::BeginListBox("Particle List", ImVec2(-FLT_MIN, 12 * ImGui::GetTextLineHeightWithSpacing())))
			{
				for (int i = 0; i < sph->particleList.size(); i++)
				{
					Particle* part = sph->particleList[i];
					std::string partName = std::format("Particle {}", i);

					if (ImGui::CollapsingHeader(partName.c_str()))
					{
						ImGui::Text("Collision Box");
						ImGui::DragFloat("Collision Box Size", &sph->collisionBoxSize, 0.01f, 0.0f, 100.0f);
						ImGui::DragFloat("Particle Elasticity", &part->elasticity, 0.01f, 0, 1.0f);

						ImGui::Text("Particle Values");
						ImGui::DragFloat3("Position", &part[0].position.x, 0.01f);
						ImGui::DragFloat3("Velocity", &part->velocity.x, 0.01f);
						ImGui::DragFloat("Density", &part->density, 1.0f, 0);
						ImGui::DragFloat("Particle Size", &part->size, 0.01f, 0.1f, 1.0f);
					}
				}
				ImGui::EndListBox();
			}
		}
	}

	ImGui::End();

	if (!_pAnnotation && _pImmediateContext)
		_pImmediateContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"ImGui Render");

	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	_pAnnotation->EndEvent();

}

void Application::Draw()
{
	float ClearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // red,green,blue,alpha
	_pImmediateContext->ClearRenderTargetView(_pRenderTargetView, ClearColor);
	_pImmediateContext->ClearDepthStencilView(_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	_pImmediateContext->OMSetRenderTargets(1, &_pRenderTargetView, _depthStencilView);

	_pImmediateContext->IASetInputLayout(_pVertexLayout);

	_pImmediateContext->VSSetShader(_pVertexShader, nullptr, 0);
	_pImmediateContext->VSSetConstantBuffers(0, 1, &_pConstantBuffer);

	_pImmediateContext->PSSetShader(_pPixelShader, nullptr, 0);
	_pImmediateContext->PSSetConstantBuffers(0, 1, &_pConstantBuffer);

	_pImmediateContext->PSSetSamplers(0, 1, &_pSamplerLinear);

	ConstantBuffer cb;

	XMFLOAT4X4 viewAsFloats = _camera->GetView();
	XMFLOAT4X4 projectionAsFloats = _camera->GetProjection();

	XMMATRIX view = XMLoadFloat4x4(&viewAsFloats);
	XMMATRIX projection = XMLoadFloat4x4(&projectionAsFloats);

	cb.View = XMMatrixTranspose(view);
	cb.Projection = XMMatrixTranspose(projection);

	cb.light = basicLight;
	cb.EyePosW = _camera->GetPosition();

	// Render all scene objects
	for (auto gameObject : m_gameObjects)
	{
		// Get render material
		Material material = gameObject->GetAppearance()->GetMaterial();

		// Copy material to shader
		cb.surface.AmbientMtrl = material.ambient;
		cb.surface.DiffuseMtrl = material.diffuse;
		cb.surface.SpecularMtrl = material.specular;

		// Set world matrix
		cb.World = XMMatrixTranspose(gameObject->GetTransform()->GetWorldMatrix());

		// Set texture
		if (gameObject->GetAppearance()->HasTexture())
		{
			ID3D11ShaderResourceView* textureRV = gameObject->GetAppearance()->GetTextureRV();
			_pImmediateContext->PSSetShaderResources(0, 1, &textureRV);
			cb.HasTexture = 1.0f;
		}
		else
		{
			cb.HasTexture = 0.0f;
		}

		// Update constant buffer
		_pImmediateContext->UpdateSubresource(_pConstantBuffer, 0, nullptr, &cb, 0, 0);

		// Draw object
		gameObject->Draw(_pImmediateContext);
	}

	_pImmediateContext->ClearRenderTargetView(_pRenderTargetView, ClearColor);
	_pImmediateContext->ClearDepthStencilView(_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	_pImmediateContext->OMSetRenderTargets(1, &_pRenderTargetView, _depthStencilView);

	if (!_pAnnotation && _pImmediateContext)
		_pImmediateContext->QueryInterface(IID_PPV_ARGS(&_pAnnotation));

	_pAnnotation->BeginEvent(L"Water Visual Batch Draw");



	_pAnnotation->EndEvent();


	_pAnnotation->BeginEvent(L"Water Simulation");

	BoundingBox box;

	XMMATRIX myWater = XMLoadFloat4x4(&m_myWater);
	cb.World = XMMatrixTranspose(myWater);
	_pImmediateContext->UpdateSubresource(_pConstantBuffer, 0, nullptr, &cb, 0, 0);

	if (isParticleVisible == true)
	{
		m_batch->Begin();
		for (int i = 0; i < min(numberOfParticlesDrawn, sph->particleList.size()); ++i)
		{
			Particle* part = sph->particleList[i];

			box.Center = part->position;
			box.Extents = XMFLOAT3(part->size, part->size, part->size);
			DrawBox(m_batch.get(), box, Colors::Blue);
		}

		//box.Center = XMFLOAT3(0, 0, 0);
		//box.Extents = XMFLOAT3(16.0f, 16.0f, 3.0f);
		//DrawBox(m_batch.get(), box, Colors::Blue);


		m_batch->End();
	}

	_pAnnotation->BeginEvent(L"Water Particles");

	sph->Draw();

	_pAnnotation->EndEvent();

	_pAnnotation->BeginEvent(L"Water Render");

	WaterBuffer wb = {};
	wb.World = XMMatrixTranspose(myWater);
	wb.View = XMMatrixTranspose(view);
	wb.Projection = XMMatrixTranspose(projection);
	_pImmediateContext->UpdateSubresource(pWaterConstantBuffer, 0, nullptr, &wb, 0, 0);

	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	_pImmediateContext->IASetVertexBuffers(0, 1, &_pWaterVertexBuffer, &stride, &offset);
	_pImmediateContext->IASetIndexBuffer(_pWaterIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	_pImmediateContext->IASetInputLayout(pWaterInputLayout);
	_pImmediateContext->VSSetShader(pWaterVertexShader, nullptr, 0);
	_pImmediateContext->VSSetConstantBuffers(0, 1, &pWaterConstantBuffer);

	_pImmediateContext->PSSetShader(pWaterPixelShader, nullptr, 0);
	_pImmediateContext->PSSetConstantBuffers(0, 1, &pWaterConstantBuffer);
	_pImmediateContext->PSSetSamplers(0, 1, &_pSamplerLinear);

	_pImmediateContext->PSSetShaderResources(0, 1, &pReflectionTextureSRV);
	_pImmediateContext->PSSetShaderResources(1, 1, &pRefractionTextureSRV);
	_pImmediateContext->PSSetShaderResources(2, 1, &pSkyBoxTextureSRV);

	_pImmediateContext->OMSetBlendState(waterBlendState, nullptr, 0xffffffff);

	_pImmediateContext->RSSetState(CWcullMode);

	_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//_pImmediateContext->DrawIndexed(6, 0, 0);

	_pAnnotation->EndEvent();

	_pAnnotation->EndEvent();

	ImGui();

	_pSwapChain->Present(0, 0);

	// Set Shader Resource to Null / Clear
	ID3D11ShaderResourceView* const shaderClear[1] = { NULL };
	for (int i = 0; i < 30; i++)
	{
		_pImmediateContext->PSSetShaderResources(i, 1, shaderClear);
	}
}