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

bool Application::HandleKeyboard(float deltaTime)
{
	float mCameraSpeed = 50.0f;        // Movement speed in units per second
	float mTurnCameraSpeed = 1.0f;   // Rotation speed in radians per second

	// Forward (W)
	if (GetAsyncKeyState('W'))
	{
		if (GetAsyncKeyState(VK_LBUTTON)) // Only move if Mouse1 is also held
		{
			currentPosX += mCameraSpeed * deltaTime * sin(rotationX) * cos(rotationY);
			currentPosZ += mCameraSpeed * deltaTime * cos(rotationX) * cos(rotationY);
			currentPosY += mCameraSpeed * deltaTime * sin(rotationY);
		}
		else
		{
			currentPosX += mCameraSpeed * deltaTime * sin(rotationX) * cos(rotationY);
			currentPosZ += mCameraSpeed * deltaTime * cos(rotationX) * cos(rotationY);
			currentPosY += mCameraSpeed * deltaTime * sin(rotationY);
		}
	}

	// Backward (S)
	if (GetAsyncKeyState('S'))
	{
		currentPosX -= mCameraSpeed * deltaTime * sin(rotationX) * cos(rotationY);
		currentPosZ -= mCameraSpeed * deltaTime * cos(rotationX) * cos(rotationY);
		currentPosY -= mCameraSpeed * deltaTime * sin(rotationY);
	}

	// Left (A - Strafe)
	if (GetAsyncKeyState('A'))
	{
		currentPosX -= mCameraSpeed * deltaTime * cos(rotationX);
		currentPosZ += mCameraSpeed * deltaTime * sin(rotationX);
	}

	// Right (D - Strafe)
	if (GetAsyncKeyState('D'))
	{
		currentPosX += mCameraSpeed * deltaTime * cos(rotationX);
		currentPosZ -= mCameraSpeed * deltaTime * sin(rotationX);
	}

	// Look Around (Mouse Movement)
	POINT mousePos;
	GetCursorPos(&mousePos);
	static POINT prevMousePos = mousePos;

	if (GetAsyncKeyState(VK_LBUTTON)) // Left Mouse Button
	{
		// Update rotation based on mouse movement
		rotationX += mTurnCameraSpeed * deltaTime * (mousePos.x - prevMousePos.x); // Horizontal look
		rotationY -= mTurnCameraSpeed * deltaTime * (mousePos.y - prevMousePos.y); // Invert vertical look

		// Clamp vertical rotation to avoid gimbal lock
		if (rotationY > XM_PIDIV2) rotationY = XM_PIDIV2; // Look up limit
		if (rotationY < -XM_PIDIV2) rotationY = -XM_PIDIV2; // Look down limit
	}

	// Update previous mouse position
	prevMousePos = mousePos;

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

	instanceData.resize(NUM_OF_PARTICLES);

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

	sph = new SPH(_pImmediateContext, _pd3dDevice);

	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\stone.dds", nullptr, &_pTextureRV);
	CreateDDSTextureFromFile(_pd3dDevice, L"Resources\\floor.dds", nullptr, &_pGroundTextureRV);

	// Setup Camera
	XMFLOAT3 eye = XMFLOAT3(0.0f, 2.0f, -1.0f);
	XMFLOAT3 at = XMFLOAT3(0.0f, 2.0f, 0.0f);
	XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	_camera = new Camera(eye, at, up, (float)_renderWidth, (float)_renderHeight, 0.01f, 10000.0f);

	// Setup the scene's light
	basicLight.AmbientLight = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	basicLight.DiffuseLight = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	basicLight.SpecularLight = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	basicLight.SpecularPower = 20.0f;
	basicLight.LightVecW = XMFLOAT3(0.0f, 1.0f, 0.0f);

	Geometry sphereGeometry;
	sphereGeometry.indexBuffer = _pIndexBuffer;
	sphereGeometry.vertexBuffer = _pVertexBuffer;
	sphereGeometry.instanceBuffer = _pInstanceBuffer;
	sphereGeometry.numberOfIndices = sphereIndices.size();
	sphereGeometry.vertexBufferOffset = 0;
	sphereGeometry.vertexBufferStride = sizeof(SimpleVertex);
	sphereGeometry.instanceBufferOffset = 0;
	sphereGeometry.instanceBufferStride = sizeof(InstanceData);

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

	D3D11_BUFFER_DESC iDesc = {};

	// Set up the buffer description for a structured buffer
	iDesc.Usage = D3D11_USAGE_DYNAMIC;
	iDesc.ByteWidth = sizeof(InstanceData) * static_cast<UINT>(instanceData.size());
	iDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	iDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	iDesc.StructureByteStride = sizeof(InstanceData);
	iDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HRESULT hr = _pd3dDevice->CreateBuffer(&iDesc, nullptr, &_pInstanceBuffer);
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create instance buffer.\n");
	}

	// Create the shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers must use UNKNOWN
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(instanceData.size());

	hr = _pd3dDevice->CreateShaderResourceView(_pInstanceBuffer, &srvDesc, &instanceBufferSRV);
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create shader resource view for instance buffer.\n");
	}

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
		// Per-vertex data
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },   // Vertex position
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },     // Vertex normal
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },       // Texture coordinates
	};

	UINT numElements = ARRAYSIZE(layout);

	_pVertexLayout = CreateInputLayout(layout, numElements, _pd3dDevice);

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

// Generate vertices and indices for a sphere
void Application::CreateSphere(float radius, int numSubdivisions, std::vector<SimpleVertex>& vertices, std::vector<WORD>& indices)
{
	const float pi = XM_PI;
	const float twoPi = 2.0f * pi;

	// Generate vertices
	for (int i = 0; i <= numSubdivisions; ++i)
	{
		// Golden Ratio
		float phi = pi * static_cast<float>(i) / numSubdivisions;

		for (int j = 0; j <= numSubdivisions; ++j)
		{
			float theta = twoPi * static_cast<float>(j) / numSubdivisions;

			SimpleVertex vertex;
			vertex.Pos.x = radius * sin(phi) * cos(theta);
			vertex.Pos.y = radius * cos(phi);
			vertex.Pos.z = radius * sin(phi) * sin(theta);

			vertex.Normal.x = vertex.Pos.x / radius;
			vertex.Normal.y = vertex.Pos.y / radius;
			vertex.Normal.z = vertex.Pos.z / radius;

			vertex.TexC.x = static_cast<float>(j) / numSubdivisions;
			vertex.TexC.y = static_cast<float>(i) / numSubdivisions;

			vertices.push_back(vertex);
		}
	}

	// Generate indices
	for (int i = 0; i < numSubdivisions; ++i) {
		for (int j = 0; j < numSubdivisions; ++j) {
			int v0 = i * (numSubdivisions + 1) + j;
			int v1 = v0 + 1;
			int v2 = (i + 1) * (numSubdivisions + 1) + j;
			int v3 = v2 + 1;

			indices.push_back(v0);
			indices.push_back(v2);
			indices.push_back(v1);

			indices.push_back(v1);
			indices.push_back(v2);
			indices.push_back(v3);
		}
	}
}

HRESULT Application::InitBuffers()
{
	HRESULT hr;

	D3D11_BUFFER_DESC vbDesc, ibDesc;
	D3D11_SUBRESOURCE_DATA vbData, ibData;

	// Vertex Buffer
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(SimpleVertex) * sphereVertices.size();
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = 0;
	vbDesc.MiscFlags = 0;
	vbData.pSysMem = sphereVertices.data();
	_pd3dDevice->CreateBuffer(&vbDesc, &vbData, &_pVertexBuffer);

	// Index Buffer
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(WORD) * sphereIndices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.MiscFlags = 0;
	ibData.pSysMem = sphereIndices.data();
	_pd3dDevice->CreateBuffer(&ibDesc, &ibData, &_pIndexBuffer);

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
	_hWnd = CreateWindow(L"WaterSim", L"Water Simulation", WS_OVERLAPPEDWINDOW,
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

	CreateSphere(1.0f, 32, sphereVertices, sphereIndices);
	InitBuffers();

	// Set primitive topology
	_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the constant buffer
	_pConstantBuffer = CreateConstantBuffer(sizeof(ConstantBuffer), _pd3dDevice, false);

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

	return hr;
}

void Application::Cleanup()
{
	if (_pImmediateContext) _pImmediateContext->ClearState();
	if (_pSamplerLinear) _pSamplerLinear->Release();

	if (_pTextureRV) _pTextureRV->Release();
	if (_pGroundTextureRV) _pGroundTextureRV->Release();
	if (_pHerculesTextureRV) _pHerculesTextureRV->Release();
	if (instanceBufferSRV) instanceBufferSRV->Release();

	if (_pConstantBuffer) _pConstantBuffer->Release();

	if (_pVertexBuffer) _pVertexBuffer->Release();
	if (_pIndexBuffer) _pIndexBuffer->Release();
	if (_pInstanceBuffer) _pInstanceBuffer->Release();


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


	delete _camera;
	_camera = nullptr;

	delete sph;
	sph = nullptr;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Application::UpdatePhysics(float deltaTime)
{
	if (SimulationControl == false)
	{
		sph->Update(deltaTime, minX, maxX);
	}
}

void Application::Update()
{
	// Update camera
	_camera->SetPosition(XMFLOAT3(currentPosX - sin(rotationX), currentPosY - sin(rotationY), currentPosZ - cos(rotationX)));
	_camera->SetLookAt(XMFLOAT3(currentPosX, currentPosY, currentPosZ));

	_camera->Update();

	// Iterate through each instance and update its position
	for (size_t i = 0; i < NUM_OF_PARTICLES; ++i)
	{
		InstanceData& instance = instanceData[i];

		// Update the position based on velocity and deltaTime
		XMStoreFloat4x4(&instance.World, XMMatrixTranspose(XMMatrixTranslation(sph->particleList[i]->position.x, sph->particleList[i]->position.y, sph->particleList[i]->position.z)));
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = _pImmediateContext->Map(_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, instanceData.data(), sizeof(InstanceData) * instanceData.size());
	_pImmediateContext->Unmap(_pInstanceBuffer, 0);
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

	ImGui::Text("Render Frames Per Second: %.1f MS / %.3f FPS", 1000.0f / io.Framerate, io.Framerate);

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

		ImGui::DragInt("Number of Particles", &particleSize);
		ImGui::DragFloat("Min X", &minX, 0.5f, -150.0f, -1.0f);
		ImGui::DragFloat("Max X", &maxX, 0.5f, 1.0f, 150.0f);
		ImGui::Checkbox("Pause", &SimulationControl);

		ImGui::Text("Initial Values");

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
						ImGui::Text("Particle Values");
						ImGui::DragFloat3("Position", &part[0].position.x, 0.01f);
						ImGui::DragFloat3("Velocity", &part->velocity.x, 0.01f);
						ImGui::DragFloat("Smoothing Radius", &part->smoothingRadius, 0.01f);
						ImGui::DragFloat("Acceleration", &part->acceleration.x, 0.01f);
						ImGui::DragFloat("Pressure", &part->pressureForce.x, 0.01f);
						ImGui::DragFloat("Density", &part->density, 0.01f);
						ImGui::DragFloat("Near Density", &part->nearDensity, 0.01f);

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

	UINT strides = sizeof(SimpleVertex);
	UINT offsets = 0;
	_pImmediateContext->IASetInputLayout(_pVertexLayout);
	_pImmediateContext->IASetVertexBuffers(0, 1, &_pVertexBuffer, &strides, &offsets);
	_pImmediateContext->IASetIndexBuffer(_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_pImmediateContext->VSSetShader(_pVertexShader, nullptr, 0);
	_pImmediateContext->VSSetConstantBuffers(0, 1, &_pConstantBuffer);
	_pImmediateContext->VSSetShaderResources(0, 1, &instanceBufferSRV);

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

	cb.surface.AmbientMtrl = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	cb.surface.DiffuseMtrl = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	cb.surface.SpecularMtrl = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);

	_pImmediateContext->PSSetShaderResources(0, 1, &_pTextureRV);
	cb.HasTexture = 0.0f;

	cb.World = XMMATRIX();

	_pImmediateContext->UpdateSubresource(_pConstantBuffer, 0, nullptr, &cb, 0, 0);
	_pImmediateContext->UpdateSubresource(_pInstanceBuffer, 0, nullptr, instanceData.data(), 0, 0);

	_pImmediateContext->DrawIndexedInstanced(sphereIndices.size(), (UINT)instanceData.size(), 0, 0, 0);

	ImGui();

	_pSwapChain->Present(0, 0);
}