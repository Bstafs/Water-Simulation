#pragma once

#include "Includes.h"
#include "SPH.h"

using namespace DirectX;

const int NUM_VERTICES_PER_RING = 32;
const int NUM_RINGS = 16;
const float RADIUS = 1.0f;

struct SurfaceInfo
{
	XMFLOAT4 AmbientMtrl;
	XMFLOAT4 DiffuseMtrl;
	XMFLOAT4 SpecularMtrl;
};

struct Light
{
	XMFLOAT4 AmbientLight;
	XMFLOAT4 DiffuseLight;
	XMFLOAT4 SpecularLight;

	float SpecularPower;
	XMFLOAT3 LightVecW;
};

struct ConstantBuffer
{
	XMMATRIX World;
	XMMATRIX View;
	XMMATRIX Projection;

	SurfaceInfo surface;

	Light light;

	XMFLOAT3 EyePosW;
	float HasTexture;
};

struct alignas(16) InstanceData
{
	XMFLOAT4X4 World;   
};

class Application
{
public:
	// Public Functions
	Application();
	~Application();

	HRESULT Initialise(HINSTANCE hInstance, int nCmdShow);

	bool HandleKeyboard(float deltaTime);

	void Update();
	void UpdatePhysics(float deltaTime);
	void Draw();

public:
	// Public Variables

private:
	// Private Functions
	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
	HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
	HRESULT InitDevice();
	void Cleanup();
	HRESULT InitShadersAndInputLayout();
	HRESULT InitBuffers();

	void CreateSphere(float radius, int numSubdivisions, std::vector<SimpleVertex>& vertices, std::vector<WORD>& indices);

	void ImGui();

private:
	// Private Variables

	// Initialise + Device
	HINSTANCE               _hInst;
	HWND                    _hWnd;
	D3D_DRIVER_TYPE         _driverType;
	D3D_FEATURE_LEVEL       _featureLevel;
	ID3D11Device* _pd3dDevice;
	ID3D11DeviceContext* _pImmediateContext;
	IDXGISwapChain* _pSwapChain;

	// Buffers
	ID3D11Buffer* _pVertexBuffer;
	ID3D11Buffer* _pIndexBuffer;
	ID3D11Buffer* _pConstantBuffer;
	ID3D11Buffer* _pInstanceBuffer;

	std::vector<SimpleVertex> sphereVertices;
	std::vector<WORD> sphereIndices;

	// Instancing
	std::vector<InstanceData> instanceData;
	ID3D11ShaderResourceView* instanceBufferSRV = nullptr;

	// Shaders + Textures
	ID3D11RenderTargetView* _pRenderTargetView;
	ID3D11VertexShader* _pVertexShader;
	ID3D11PixelShader* _pPixelShader;
	ID3D11InputLayout* _pVertexLayout;

	ID3D11DepthStencilView* _depthStencilView = nullptr;
	ID3D11Texture2D* _depthStencilBuffer = nullptr;
	ID3D11DepthStencilState* DSLessEqual;
	ID3D11RasterizerState* RSCullNone;
	ID3D11RasterizerState* CCWcullMode;
	ID3D11RasterizerState* CWcullMode;

	ID3D11ShaderResourceView* _pTextureRV = nullptr;
	ID3D11ShaderResourceView* _pGroundTextureRV = nullptr;
	ID3D11ShaderResourceView* _pHerculesTextureRV = nullptr;

	ID3D11SamplerState* _pSamplerLinear = nullptr;

	// Classes
	MeshData objMeshData;
	Light basicLight;
	Camera* _camera = nullptr;
	Transform* m_transform;

	// Window
	UINT _WindowWidth = 1920;
	UINT _WindowHeight = 1080;

	UINT _renderWidth = _WindowWidth;
	UINT _renderHeight = _WindowHeight;

	// Camera
	float currentPosZ = -30.0f;
	float currentPosX = 0.0f;
	float currentPosY = 5.0f;
	float rotationX = 0.0f;
	float rotationY = -0.4f;

	// Collision Box
	float minX = -50.0f, maxX = 50.0f;
	float minZ = -15.0f, maxZ = 15.0f;

	// SPH
	std::unique_ptr<SPH> sph;


	bool SimulationControl = true;

	ID3DUserDefinedAnnotation* _pAnnotation = nullptr;
};

