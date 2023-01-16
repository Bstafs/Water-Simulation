#include "Plane.h"
using namespace std;
using namespace DirectX;

#define NUM_VERTICES 36

Plane::Plane()
{
	m_pVertexBufferPlane = nullptr;
	m_pIndexBufferPlane = nullptr;
	m_pTextureResourceView = nullptr;
	m_pNormalResourceView = nullptr;
	m_pParraResourceView = nullptr;
	m_pSamplerLinear = nullptr;

	// Initialize the world matrix
	XMStoreFloat4x4(&m_World, XMMatrixIdentity());
}


Plane::~Plane()
{
	cleanup();
}

void Plane::cleanup()
{
	if (m_pVertexBufferPlane)
		m_pVertexBufferPlane->Release();
	m_pVertexBufferPlane = nullptr;

	if (m_pIndexBufferPlane)
		m_pIndexBufferPlane->Release();
	m_pIndexBufferPlane = nullptr;

	if (m_pTextureResourceView)
		m_pTextureResourceView->Release();
	m_pTextureResourceView = nullptr;

	if (m_pNormalResourceView)
		m_pNormalResourceView->Release();
	m_pNormalResourceView = nullptr;

	if (m_pParraResourceView)
		m_pParraResourceView->Release();
	m_pParraResourceView = nullptr;

	if (m_pSamplerLinear)
		m_pSamplerLinear->Release();
	m_pSamplerLinear = nullptr;

	if (m_pMaterialConstantBuffer)
		m_pMaterialConstantBuffer->Release();
	m_pMaterialConstantBuffer = nullptr;
}


HRESULT Plane::initMesh(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext)
{
	// Create vertex buffer
	SimpleVertexPlane vertices[] =
	{
	{ XMFLOAT3(-10.0f, 0.0f, -10.0f),XMFLOAT3(0.0f, 1.0f, 0.0f) ,XMFLOAT2(0.0f, 1.0f)}, // 0 bottom left  

	{ XMFLOAT3(-10.0f, 0.0f, 10.0f),XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 1  top left

	{ XMFLOAT3(10.0f, 0.0f, 10.0f),XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 2 top right

	{ XMFLOAT3(10.0f, 0.0f, -10.0f),XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 3 bottom right
	};

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertexPlane) * 6;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;
	HRESULT hr = pd3dDevice->CreateBuffer(&bd, &InitData, &m_pVertexBufferPlane);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertexPlane);
	UINT offset = 0;
	pContext->IASetVertexBuffers(0, 1, &m_pVertexBufferPlane, &stride, &offset);

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
	hr = pd3dDevice->CreateBuffer(&bd, &InitData, &m_pIndexBufferPlane);
	if (FAILED(hr))
		return hr;


	// Set index buffer
	pContext->IASetIndexBuffer(m_pIndexBufferPlane, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// load and setup textures
	hr = CreateDDSTextureFromFile(pd3dDevice, L"Resources\\tiles.dds", nullptr, &m_pTextureResourceView);
	if (FAILED(hr))
		return hr;

	//hr = CreateDDSTextureFromFile(pd3dDevice, L"Resources\\normals.dds", nullptr, &m_pNormalResourceView);
	//if (FAILED(hr))
	//	return hr;

	//hr = CreateDDSTextureFromFile(pd3dDevice, L"Resources\\displacement.dds", nullptr, &m_pParraResourceView);
	//if (FAILED(hr))
	//	return hr;

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = pd3dDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);

	m_material.Material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_material.Material.Specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_material.Material.SpecularPower = 32.0f;
	m_material.Material.UseTexture = true;
	m_material.Material.useNormals = false;
	m_material.Material.useParallax = false;


	// Create the material constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(MaterialPropertiesConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = pd3dDevice->CreateBuffer(&bd, nullptr, &m_pMaterialConstantBuffer);
	if (FAILED(hr))
		return hr;

	return hr;
}

void Plane::setPosition(XMFLOAT3 position)
{
	m_positionPlane = position;
}

void Plane::update(float t, ID3D11DeviceContext* pContext)
{
	static float cummulativeTime = 0;
	cummulativeTime += t;

	// Cube:  Rotate around origin
	//XMMATRIX mSpin = XMMatrixRotationY(cummulativeTime);
	XMMATRIX mRotationX = XMMatrixRotationX(m_rotationPlane.x);
	XMMATRIX mRotationY = XMMatrixRotationY(m_rotationPlane.y);
	XMMATRIX mRotationZ = XMMatrixRotationZ(m_rotationPlane.z);
	XMMATRIX mSpin = XMMatrixMultiply(XMMatrixRotationX(cummulativeTime), XMMatrixRotationY(cummulativeTime));
	XMMATRIX mTranslate = XMMatrixTranslation(m_positionPlane.x, m_positionPlane.y, m_positionPlane.z);
	XMMATRIX world = mTranslate;
	if (isSpinning == false)
	{
		world = mTranslate * mRotationX * mRotationY * mRotationZ;
	}
	else if (isSpinning == true)
	{
		world = mTranslate * mSpin;
	}
	XMStoreFloat4x4(&m_World, world);

	pContext->UpdateSubresource(m_pMaterialConstantBuffer, 0, nullptr, &m_material, 0, 0);
}

void Plane::draw(ID3D11DeviceContext* pContext)
{
	pContext->PSSetShaderResources(1, 1, &m_pNormalResourceView);
	pContext->PSSetShaderResources(2, 1, &m_pParraResourceView);
	pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	pContext->DrawIndexed(6, 0, 0);
}
