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
	float mCameraSpeed = 0.008f;
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

	sph = new SPH(NUM_OF_PARTICLES, _pImmediateContext, _pd3dDevice);

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
	sphereGeometry.numberOfIndices = sphereIndices.size();
	sphereGeometry.vertexBufferOffset = 0;
	sphereGeometry.vertexBufferStride = sizeof(SimpleVertex);

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


	for (int i = 0; i < sph->particleList.size(); i++)
	{
		GameObject* gameObject = new GameObject("Sphere " + i, sphereGeometry, shinyMaterial);
		gameObject->GetTransform()->SetScale(1.0f, 1.0f, 1.0f);
		gameObject->GetTransform()->SetPosition(sph->particleList[i]->position.x, sph->particleList[i]->position.y, sph->particleList[i]->position.z);
		gameObject->GetAppearance()->SetTextureRV(0);
		m_gameObjects.push_back(gameObject);
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
	for (int i = 0; i <= numSubdivisions; ++i) {
		float phi = pi * static_cast<float>(i) / numSubdivisions;

		for (int j = 0; j <= numSubdivisions; ++j) {
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

HRESULT Application::InitVertexBuffer()
{
	HRESULT hr;

	D3D11_BUFFER_DESC vbDesc, ibDesc;
	D3D11_SUBRESOURCE_DATA vbData, ibData;

	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(SimpleVertex) * sphereVertices.size();
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = 0;
	vbDesc.MiscFlags = 0;
	vbData.pSysMem = sphereVertices.data();
	_pd3dDevice->CreateBuffer(&vbDesc, &vbData, &_pVertexBuffer);

	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(WORD) * sphereIndices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.MiscFlags = 0;
	ibData.pSysMem = sphereIndices.data();
	_pd3dDevice->CreateBuffer(&ibDesc, &ibData, &_pIndexBuffer);

	return S_OK;
}

HRESULT Application::InitIndexBuffer()
{
	HRESULT hr;

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
	InitVertexBuffer();
	InitIndexBuffer();

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

	if (_pConstantBuffer) _pConstantBuffer->Release();

	if (_pVertexBuffer) _pVertexBuffer->Release();
	if (_pIndexBuffer) _pIndexBuffer->Release();

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

void Application::UpdatePhysics(float deltaTime)
{
	if (SimulationControl == false)
	{
		sph->Update(deltaTime, minX, maxX);
	}

	for (auto gameObject : m_gameObjects)
	{
		gameObject->Update(deltaTime);
	}
}

void Application::Update()
{
	// Update camera
	_camera->SetPosition(XMFLOAT3(currentPosX - sin(rotationX), currentPosY - sin(rotationY), currentPosZ - cos(rotationX)));
	_camera->SetLookAt(XMFLOAT3(currentPosX, currentPosY, currentPosZ));

	_camera->Update();

	for (int i = 0; i < m_gameObjects.size(); ++i)
	{
		m_gameObjects[i]->GetTransform()->SetPosition(sph->particleList[i]->position);
	}
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

		ImGui::DragInt("Number of Particles", &particleSize);
		ImGui::DragFloat("Min X", &minX, 0.5f, -50.0f, -1.0f);
		ImGui::DragFloat("Max X", &maxX, 0.5f, 1.0f, 50.0f);
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

	ImGui();

	_pSwapChain->Present(0, 0);

	// Set Shader Resource to Null / Clear
	ID3D11ShaderResourceView* const shaderClear[1] = { NULL };
	for (int i = 0; i < 1; i++)
	{
		_pImmediateContext->PSSetShaderResources(i, 1, shaderClear);
	}
}