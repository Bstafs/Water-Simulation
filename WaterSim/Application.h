#pragma once

#include "Includes.h"
#include "SPH.h"
#include "Timestep.h"

#define NUMBER_OF_CUBES 1
#define FPS_60 1.0f/60.0f
#define THREAD_COUNT 32;
using namespace DirectX;

using VertexType = DirectX::VertexPositionColor;

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

struct WaterBuffer
{
	XMMATRIX World;
	XMMATRIX View;
	XMMATRIX Projection;
};


class Application
{
private:
	HINSTANCE               _hInst;
	HWND                    _hWnd;
	D3D_DRIVER_TYPE         _driverType;
	D3D_FEATURE_LEVEL       _featureLevel;
	ID3D11Device* _pd3dDevice;
	ID3D11DeviceContext* _pImmediateContext;
	IDXGISwapChain* _pSwapChain;
	ID3D11RenderTargetView* _pRenderTargetView;
	ID3D11VertexShader* _pVertexShader;
	ID3D11PixelShader* _pPixelShader;
	ID3D11InputLayout* _pVertexLayout;

	// Vertex Buffers
	ID3D11Buffer* _pVertexBuffer;
	ID3D11Buffer* _pIndexBuffer;

	ID3D11Buffer* _pConstantBuffer;

	ID3D11DepthStencilView* _depthStencilView = nullptr;
	ID3D11Texture2D* _depthStencilBuffer = nullptr;

	ID3D11ShaderResourceView* _pTextureRV = nullptr;

	ID3D11ShaderResourceView* _pGroundTextureRV = nullptr;

	ID3D11ShaderResourceView* _pHerculesTextureRV = nullptr;

	ID3D11SamplerState* _pSamplerLinear = nullptr;

	std::vector<SimpleVertex> sphereVertices;
	std::vector<WORD> sphereIndices;

	MeshData objMeshData;

	Light basicLight;

	vector<GameObject*> m_gameObjects;

	Camera* _camera = nullptr;

	UINT _WindowWidth = 1920;
	UINT _WindowHeight = 1080;

	// Render dimensions - Change here to alter screen resolution
	UINT _renderWidth = _WindowWidth;
	UINT _renderHeight = _WindowHeight;

	float currentPosZ = -30.0f;
	float currentPosX = 0.0f;
	float currentPosY = 0.0f;
	float rotationX = 0.0f;
	float rotationY = 0.0f;

	ID3D11DepthStencilState* DSLessEqual;
	ID3D11RasterizerState* RSCullNone;

	ID3D11RasterizerState* CCWcullMode;
	ID3D11RasterizerState* CWcullMode;

	Transform* m_transform;

	bool ToggleCollisionsMode = true;

	double lastTime;
	double timer;
	double accumulator;
	double nowTime;
	int frames;
	int updates;

	// SPH

	SPH* sph = nullptr;


	ID3DUserDefinedAnnotation* _pAnnotation = nullptr;

	Timestep* _timestep;

private:
	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
	HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
	HRESULT InitDevice();
	void Cleanup();
	HRESULT InitShadersAndInputLayout();
	HRESULT InitVertexBuffer();
	HRESULT InitIndexBuffer();

	void CreateSphere(float radius, int numSubdivisions, std::vector<SimpleVertex>& vertices, std::vector<WORD>& indices);

	void ImGui();
	float CalculateDeltaTime60FPS();

	void moveForward(int objectNumber);
	void moveBackward(int objectNumber);
	void moveLeft(int objectNumber);
	void moveRight(int objectNumber);

	std::unique_ptr<DirectX::PrimitiveBatch<VertexType>> m_batch;
public:
	Application();
	~Application();

	HRESULT Initialise(HINSTANCE hInstance, int nCmdShow);

	bool HandleKeyboard();

	void Update();
	void UpdatePhysics();
	void Draw();
};

