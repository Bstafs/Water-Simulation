#pragma once

#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "DDSTextureLoader.h"
#include "resource.h"
#include "Camera.h"
#include "Structures.h"
#include "OBJLoader.h"
#include <vector>
#include "Quaternion.h"
#include "GameObject.h"

#include "SPH.h"

#include"imgui/imgui.h"
#include"imgui/imgui_impl_dx11.h"
#include"imgui/imgui_impl_win32.h"

#define NUMBER_OF_CUBES 2
#define FPS_60 1.0f/60.0f
#define THREAD_COUNT 32;
using namespace DirectX;

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

struct ParticleConstantBuffer
{
	int particleCount;
	XMFLOAT3 padding00;

	float deltaTime;
	float smoothingLength;
	float pressure;
	float restDensity;

	float densityCoef;
	float GradPressureCoef;
	float LapViscosityCoef;
	float gravity;
};

struct ConstantParticleData
{
	XMFLOAT3 position;
	float pressure;

	XMFLOAT3 velocity;
	float density;

	XMFLOAT3 force;
	float padding01;

	XMFLOAT3 acceleration;
	float padding02;
};

struct ParticleData
{
	XMFLOAT3 position;
	float pressure;

	XMFLOAT3 velocity;
	float density;
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

	// Compute Shaders
	ID3D11ComputeShader* _pComputeShader = nullptr;

	ID3D11ShaderResourceView* _ppSRVNULL[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* _ppUAVViewNULL[1] = { nullptr };

	ID3D11UnorderedAccessView* _pOutputUAV = nullptr;
	ID3D11ShaderResourceView* _pInputSRV = nullptr;

	ID3D11Buffer* _pParticleConstantBuffer = nullptr;
	ID3D11Buffer* _pInputComputeBuffer = nullptr;
	ID3D11Buffer* _pOutputComputeBuffer = nullptr;
	ID3D11Buffer* _pOutputResultComputeBuffer = nullptr;

	// Vertex Buffers
	ID3D11Buffer* _pVertexBuffer;
	ID3D11Buffer* _pIndexBuffer;

	ID3D11Buffer* _pPlaneVertexBuffer;
	ID3D11Buffer* _pPlaneIndexBuffer;

	ID3D11Buffer* _pConstantBuffer;

	ID3D11DepthStencilView* _depthStencilView = nullptr;
	ID3D11Texture2D* _depthStencilBuffer = nullptr;

	ID3D11ShaderResourceView* _pTextureRV = nullptr;

	ID3D11ShaderResourceView* _pGroundTextureRV = nullptr;

	ID3D11ShaderResourceView* _pHerculesTextureRV = nullptr;

	ID3D11SamplerState* _pSamplerLinear = nullptr;

	ParticleConstantBuffer pcb;

	MeshData objMeshData;

	Light basicLight;

	vector<GameObject*> m_gameObjects;

	Camera* _camera = nullptr;

	UINT _WindowWidth = 1920;
	UINT _WindowHeight = 1080;

	// Render dimensions - Change here to alter screen resolution
	UINT _renderWidth = _WindowWidth;
	UINT _renderHeight = _WindowHeight;

	float currentPosZ = 0.0f;
	float currentPosX = 0.0f;
	float currentPosY = 2.0f;
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

	// Particle Variables for ImGui
	int numbParticles;
	float mass;
	float density;
	float gasConstant;
	float viscosity;
	float h;
	float g = 9.0f;
	float tension;
	float elastisicty;

private:
	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
	HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
	HRESULT InitDevice();
	void Cleanup();
	HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);
	HRESULT CompileComputeShader(_In_ LPCWSTR fileName, _In_ LPCSTR entryPoint, _In_ ID3D11Device* device, _Outptr_ ID3DBlob** blob);
	HRESULT InitShadersAndInputLayout();
	HRESULT InitVertexBuffer();
	HRESULT InitIndexBuffer();

	void ImGui();
	float CalculateDeltaTime60FPS();

	void moveForward(int objectNumber);
	void moveBackward(int objectNumber);
	void moveLeft(int objectNumber);
	void moveRight(int objectNumber);

public:
	Application();
	~Application();

	HRESULT Initialise(HINSTANCE hInstance, int nCmdShow);

	bool HandleKeyboard();

	void Update();
	void UpdatePhysics(float deltaTime);
	void Draw();
};

