#include "Application.h"

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
	float mCameraSpeed = 0.008f;

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

	numbParticles = 17;
	mass = 0.02f;
	density = 997.0f;
	gasConstant = 1.0f;
	viscosity = 1.04f;
	h = 1.0f;
	g = -9.807f;
	tension = 0.2f;
	elastisicty = 0.5f;
	sph = new SPH(numbParticles, mass, density, gasConstant, viscosity, h, g, tension, elastisicty, 200.0f);
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

	// Setup Camera
	XMFLOAT3 eye = XMFLOAT3(0.0f, 2.0f, -1.0f);
	XMFLOAT3 at = XMFLOAT3(0.0f, 2.0f, 0.0f);
	XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	_camera = new Camera(eye, at, up, (float)_renderWidth, (float)_renderHeight, 0.01f, 200.0f);

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
	gameObject->GetParticleModel()->SetMass(1.0f);
	gameObject->GetAppearance()->SetTextureRV(_pGroundTextureRV);
	gameObject->GetParticleModel()->SetToggleGravity(false);
	m_gameObjects.push_back(gameObject);

	for (int i = 0; i < NUMBER_OF_CUBES; i++)
	{
		gameObject = new GameObject("Cube " + i, cubeGeometry, shinyMaterial);
		gameObject->GetTransform()->SetScale(0.5f, 0.5f, 0.5f);
		gameObject->GetTransform()->SetPosition(-4.0f + (i * 2.0f), 0.5f, 10.0f);
		//	gameObject->GetTransform()->SetPosition(sph->particleList[i]->position.x, sph->particleList[i]->position.y, sph->particleList[i]->position.z);
		gameObject->GetAppearance()->SetTextureRV(_pTextureRV);
		gameObject->GetParticleModel()->SetToggleGravity(false);
		gameObject->GetParticleModel()->SetMass(1.0f);
		gameObject->GetParticleModel()->SetAcceleration(0.0f, 0.0f, 0.0f);
		gameObject->GetParticleModel()->SetNetForce(0.0f, 0.0f, 0.0f);
		gameObject->GetParticleModel()->SetVelocity(0.0f, 0.0f, 0.0f);
		gameObject->GetParticleModel()->SetDrag(4.0f, 4.0f, 4.0f);
		gameObject->GetRigidBody()->SetAngularVelocity(0.0f, 0.0f, 0.0f);
		m_gameObjects.push_back(gameObject);
	}

	return S_OK;
}

HRESULT Application::InitShadersAndInputLayout()
{
	HRESULT hr;

	// Compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"shader.fx", "VS", "vs_4_0", &pVSBlob);

	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = _pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &_pVertexShader);

	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"shader.fx", "PS", "ps_4_0", &pPSBlob);

	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = _pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &_pPixelShader);
	pPSBlob->Release();

	if (FAILED(hr))
		return hr;

	// Compile Compute Shader
	ID3DBlob* csBlob = nullptr;
	hr = CompileComputeShader(L"SPHComputeShader.hlsl", "CSMain", _pd3dDevice, &csBlob);
	if (FAILED(hr))
	{
		_pd3dDevice->Release();
		printf("Failed compiling shader %08X\n", hr);
		return -1;
	}

	// Create Compute Shader
	hr = _pd3dDevice->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &_pComputeShader);
	csBlob->Release();

	if (FAILED(hr))
	{
		_pd3dDevice->Release();
		return hr;
	}

	OutputDebugStringA("Success");

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = _pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &_pVertexLayout);
	pVSBlob->Release();

	if (FAILED(hr))
		return hr;

	// Set the input layout
	_pImmediateContext->IASetInputLayout(_pVertexLayout);

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

HRESULT Application::CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);

	if (FAILED(hr))
	{
		if (pErrorBlob != nullptr)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

		if (pErrorBlob) pErrorBlob->Release();

		return hr;
	}

	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

HRESULT Application::CompileComputeShader(LPCWSTR fileName, LPCSTR entryPoint, ID3D11Device* device, ID3DBlob** blob)
{
	if (!fileName || !entryPoint || !device || !blob)
	{
		return E_INVALIDARG;
	}

	*blob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined (DEBUG) || defined (_DEBUG)
	flags |= D3DCOMPILE_DEBUG;
#endif

	LPCSTR profile = (device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";

	const D3D_SHADER_MACRO define[] =
	{
		"DEFINE:", "1",
		NULL, NULL
	};

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3DCompileFromFile(fileName, define, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile, flags, 0, &shaderBlob, &errorBlob);

	if (FAILED(hr))
	{
		if (FAILED(errorBlob))
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}

		if (shaderBlob)
		{
			shaderBlob->Release();
		}

		return hr;
	}

	*blob = shaderBlob;

	return hr;
}

HRESULT Application::InitDevice()
{
	HRESULT hr = S_OK;

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
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = _pd3dDevice->CreateBuffer(&bd, nullptr, &_pConstantBuffer);

	if (FAILED(hr))
		return hr;

	// Create the constant buffer
	D3D11_BUFFER_DESC pcbDesc;
	ZeroMemory(&pcbDesc, sizeof(pcbDesc));
	pcbDesc.Usage = D3D11_USAGE_DEFAULT;
	pcbDesc.ByteWidth = sizeof(ParticleConstantBuffer);
	pcbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pcbDesc.CPUAccessFlags = 0;
	pcbDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA sb = {};
	sb.pSysMem = &pcb;

	hr = _pd3dDevice->CreateBuffer(&pcbDesc, &sb, &_pParticleConstantBuffer);

	if (FAILED(hr))
		return hr;

	// COMPUTE SHADERS

	// Input Buffer

	// Create Input Buffer
	D3D11_BUFFER_DESC constantDataDesc;
	constantDataDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantDataDesc.ByteWidth = sizeof(ConstantParticleData) * sph->particleList.size();
	constantDataDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	constantDataDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantDataDesc.StructureByteStride = sizeof(ConstantParticleData);
	constantDataDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = _pd3dDevice->CreateBuffer(&constantDataDesc, nullptr, &_pInputComputeBuffer);

	// Create Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	shaderResourceViewDesc.BufferEx.FirstElement = 0;
	shaderResourceViewDesc.BufferEx.Flags = 0;
	shaderResourceViewDesc.BufferEx.NumElements = sph->particleList.size();
	hr = _pd3dDevice->CreateShaderResourceView(_pInputComputeBuffer, &shaderResourceViewDesc, &_pInputSRV);

	// Output Buffer
	D3D11_BUFFER_DESC outputDesc;
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.ByteWidth = sizeof(ParticleData) * sph->particleList.size();
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.StructureByteStride = sizeof(ParticleData);
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = _pd3dDevice->CreateBuffer(&outputDesc, nullptr, &_pOutputComputeBuffer);

	// System Memory Version to read results
	outputDesc.Usage = D3D11_USAGE_STAGING;
	outputDesc.BindFlags = 0;
	outputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = _pd3dDevice->CreateBuffer(&outputDesc, nullptr, &_pOutputResultComputeBuffer);

	// Create UAV
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = sph->particleList.size();
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	hr = _pd3dDevice->CreateUnorderedAccessView(_pOutputComputeBuffer, &uavDesc, &_pOutputUAV);

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

	return S_OK;
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

	if (_pComputeShader) _pComputeShader->Release();
	if (_pOutputUAV) _pOutputUAV->Release();
	if (_pInputSRV) _pInputSRV->Release();
	if (_pInputComputeBuffer) _pInputComputeBuffer->Release();
	if (_pOutputComputeBuffer)_pOutputComputeBuffer->Release();
	if (_pOutputResultComputeBuffer) _pOutputResultComputeBuffer->Release();

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

void Application::moveForward(int objectNumber)
{
	Vector3 accel = m_gameObjects[objectNumber]->GetParticleModel()->GetAcceleration();
	Vector3 velocity = m_gameObjects[objectNumber]->GetParticleModel()->GetVelocity();
	accel.z -= 0.02f;
	velocity.z -= 0.02f;
	m_gameObjects[objectNumber]->GetParticleModel()->SetAcceleration(accel);
	m_gameObjects[objectNumber]->GetParticleModel()->SetVelocity(velocity);
}

void Application::moveBackward(int objectNumber)
{
	Vector3 accel = m_gameObjects[objectNumber - 2]->GetParticleModel()->GetAcceleration();
	Vector3 velocity = m_gameObjects[objectNumber - 2]->GetParticleModel()->GetVelocity();
	accel.z -= 0.02f;
	velocity.z += 0.02f;
	m_gameObjects[objectNumber - 2]->GetParticleModel()->SetAcceleration(accel);
	m_gameObjects[objectNumber - 2]->GetParticleModel()->SetVelocity(velocity);
}

void Application::moveLeft(int objectNumber)
{
	Vector3 velocity = m_gameObjects[objectNumber - 2]->GetParticleModel()->GetVelocity();
	velocity.x -= 0.02f;
	m_gameObjects[objectNumber - 2]->GetParticleModel()->SetVelocity(velocity);
}

void Application::moveRight(int objectNumber)
{
	Vector3 velocity = m_gameObjects[objectNumber - 2]->GetParticleModel()->GetVelocity();
	velocity.x += 0.02f;
	m_gameObjects[objectNumber - 2]->GetParticleModel()->SetVelocity(velocity);
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

	for (auto gameObject : m_gameObjects)
	{
		gameObject->Update(deltaTime);
	}

	for (int i = 0; i < sph->particleList.size(); i++)
	{
		for (GameObject* go : m_gameObjects)
		{
			go->GetTransform()->SetPosition(sph->particleList[0]->position.x, sph->particleList[0]->position.y, sph->particleList[0]->position.z);
		}
	}



	// Move gameobject Forces

	if (GetAsyncKeyState('1'))
	{
		moveForward(1);
	}
	if (GetAsyncKeyState('2'))
	{
		moveForward(2);
	}
	if (GetAsyncKeyState('3'))
	{
		moveBackward(3);
	}
	if (GetAsyncKeyState('4'))
	{
		moveBackward(4);
	}

	// Move Objects Collisons

	if (GetAsyncKeyState('5'))
	{
		moveLeft(5);
	}
	if (GetAsyncKeyState('6'))
	{
		moveLeft(6);
	}
	if (GetAsyncKeyState('7'))
	{
		moveRight(5);
	}
	if (GetAsyncKeyState('8'))
	{
		moveRight(6);
	}

	if (GetAsyncKeyState('J'))
	{
		m_gameObjects[5]->GetRigidBody()->CalculateTorque(Vector3(0.0f, 0.0f, -1.0f), Vector3(0.5f, 0.0f, 0.0f), deltaTime); // Front Face, Left Edge
	}
	if (GetAsyncKeyState('K'))
	{
		m_gameObjects[5]->GetRigidBody()->CalculateTorque(Vector3(0.0f, 0.0f, -1.0f), Vector3(0.0f, -0.5f, 0.0f), deltaTime); //  Front Face, Top Edge
	}
	if (GetAsyncKeyState('L'))
	{
		m_gameObjects[5]->GetRigidBody()->CalculateTorque(Vector3(0.0f, 0.0f, 1.0f), Vector3(0.5f, 0.0f, 0.0f), deltaTime); //  Front Face, Right Edge
	}

	if (GetAsyncKeyState('T'))
	{
		ToggleCollisionsMode = true;
	}
	if (GetAsyncKeyState('Y'))
	{
		ToggleCollisionsMode = false;
	}

	sph->Update(*sph, deltaTime);

	//if (GetAsyncKeyState('5') && 0x8000)
	//{
	//	Debug::GetInstance().DebugWrite("KeyPressed\n");
	//}

	// Update camera

	_camera->SetPosition(XMFLOAT3(currentPosX - sin(rotationX), currentPosY - sin(rotationY), currentPosZ - cos(rotationX)));
	_camera->SetLookAt(XMFLOAT3(currentPosX, currentPosY, currentPosZ));

	_camera->Update();


	// Update objects

	//m_gameObjects[6]->GetParticleModel()->SetToggleGravity(true);
}

void Application::ImGui()
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
	if (ImGui::CollapsingHeader("SPH"))
	{
		ImGui::Text("Initial Values");
		ImGui::DragInt("Number Of Particles", &sph->numberOfParticles);
		ImGui::DragFloat("Mass", &sph->MASS_CONSTANT);
		ImGui::DragFloat("Density", &sph->sphDensity);
		ImGui::DragFloat("Gas Constant", &sph->GAS_CONSTANT);
		ImGui::DragFloat("Viscosity", &sph->sphViscosity);
		ImGui::DragFloat("Core Radius", &sph->sphH);
		ImGui::DragFloat("Gravity", &sph->sphG);
		ImGui::DragFloat("Tension", &sph->sphTension);

		ImGui::Text("Particle Values");
		ImGui::DragFloat3("Position", &sph->tempPositionValue.x);
		ImGui::DragFloat3("Velocity", &sph->tempVelocityValue.x);
		ImGui::DragFloat("Density", &sph->tempDensityValue);
		ImGui::DragFloat("Particle Size", &sph->particleList[0]->size, 0.01f, 0.1f, 1.0f);
	}

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Application::Draw()
{
	//
	// Clear buffers
	//

	float ClearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // red,green,blue,alpha
	_pImmediateContext->ClearRenderTargetView(_pRenderTargetView, ClearColor);
	_pImmediateContext->ClearDepthStencilView(_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	//
	// Setup buffers and render scene
	//

	_pImmediateContext->IASetInputLayout(_pVertexLayout);

	_pImmediateContext->VSSetShader(_pVertexShader, nullptr, 0);
	_pImmediateContext->VSSetConstantBuffers(0, 1, &_pConstantBuffer);

	_pImmediateContext->PSSetShader(_pPixelShader, nullptr, 0);
	_pImmediateContext->PSSetConstantBuffers(0, 1, &_pConstantBuffer);

	_pImmediateContext->PSSetSamplers(0, 1, &_pSamplerLinear);

	pcb.particleCount = sph->particleList.size();
	pcb.padding00 = XMFLOAT3(0.0f, 0.0f, 0.0f);
	pcb.deltaTime = 1.0f / 60.0f;
	pcb.smoothingLength = h;
	pcb.pressure = 200.0f;
	pcb.restDensity = density;
	pcb.densityCoef = sph->POLY6_CONSTANT;
	pcb.GradPressureCoef = sph->SPIKYGRAD_CONSTANT;
	pcb.LapViscosityCoef = sph->VISC_CONSTANT;
	pcb.gravity = g;
	_pImmediateContext->UpdateSubresource(_pParticleConstantBuffer, 0, nullptr, &pcb, 0, 0);

	// Compute Shader Input Buffer
	ConstantParticleData pd;
	pd.position = sph->GetPosition();
	pd.pressure= 200.0f;
	pd.velocity = sph->GetVelocity();
	pd.density = density;
	pd.force = sph->GetForce();
	pd.padding01 = 0.0f;
	pd.acceleration = sph->GetAccel();
	pd.padding02 = 0.0f;

	// Map Values using the Input Buffer
	D3D11_MAPPED_SUBRESOURCE pdMS;
	_pImmediateContext->Map(_pInputComputeBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &pdMS);
	memcpy(pdMS.pData, &pd, sizeof(ConstantParticleData));
	_pImmediateContext->Unmap(_pInputComputeBuffer, 0);

	// Set Compute Shader
	_pImmediateContext->CSSetShader(_pComputeShader, nullptr, 0);
	_pImmediateContext->CSSetShaderResources(0, 1, &_pInputSRV);
	_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &_pOutputUAV, nullptr);
	// Set Buffer Sending over data
	_pImmediateContext->CSSetConstantBuffers(1, 1, &_pParticleConstantBuffer);
	_pImmediateContext->CSSetConstantBuffers(0, 1, &_pInputComputeBuffer);
	// Dispatch Shader
	_pImmediateContext->Dispatch(256, 1, 1);
	// Set Shader to Null
	_pImmediateContext->CSSetShader(nullptr, nullptr, 0);
	_pImmediateContext->CSSetUnorderedAccessViews(0, 1, _ppUAVViewNULL, nullptr);
	_pImmediateContext->CSSetShaderResources(0, 2, _ppSRVNULL);
	// Copy Results of Output Buffer
	_pImmediateContext->CopyResource(_pOutputResultComputeBuffer, _pOutputComputeBuffer);

	// Map the Output Buffer
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = _pImmediateContext->Map(_pOutputResultComputeBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

	if (SUCCEEDED(hr))
	{
		ParticleData* dataView = reinterpret_cast<ParticleData*>(mappedResource.pData);


		sph->tempPositionValue = dataView->position;
		sph->tempVelocityValue = dataView->velocity;
		sph->tempDensityValue = dataView->density;
		sph->tempForceValue = dataView->force;
		sph->tempPressureValue = dataView->pressure;

		_pImmediateContext->Unmap(_pOutputResultComputeBuffer, 0);
	}

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

	ImGui();

	sph->Draw();

	//
	// Present our back buffer to our front buffer
	//
	_pSwapChain->Present(0, 0);
}