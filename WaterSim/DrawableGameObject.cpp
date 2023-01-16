#include "DrawableGameObject.h"

using namespace std;
using namespace DirectX;

#define NUM_VERTICES 36

DrawableGameObject::DrawableGameObject()
{
	m_pVertexBuffer = nullptr;
	m_pIndexBuffer = nullptr;
	m_pTextureResourceView = nullptr;
	m_pNormalResourceView = nullptr;
	m_pParraResourceView = nullptr;
	m_pSamplerLinear = nullptr;

	// Initialize the world matrix
	XMStoreFloat4x4(&m_World, XMMatrixIdentity());
}


DrawableGameObject::~DrawableGameObject()
{
	cleanup();
}

void DrawableGameObject::cleanup()
{
	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();
	m_pVertexBuffer = nullptr;

	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();
	m_pIndexBuffer = nullptr;

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

void CalculateTangentBinormalLH(SimpleVertex v0, SimpleVertex v1, SimpleVertex v2, XMFLOAT3& normal, XMFLOAT3& tangent, XMFLOAT3& binormal)
{
	XMFLOAT3 edge1(v1.Pos.x - v0.Pos.x, v1.Pos.y - v0.Pos.y, v1.Pos.z - v0.Pos.z);
	XMFLOAT3 edge2(v2.Pos.x - v0.Pos.x, v2.Pos.y - v0.Pos.y, v2.Pos.z - v0.Pos.z);

	XMFLOAT2 deltaUV1(v1.TexCoord.x - v0.TexCoord.x, v1.TexCoord.y - v0.TexCoord.y);
	XMFLOAT2 deltaUV2(v2.TexCoord.x - v0.TexCoord.x, v2.TexCoord.y - v0.TexCoord.y);

	float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

	tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
	XMVECTOR tn = XMLoadFloat3(&tangent);
	tn = XMVector3Normalize(tn);
	XMStoreFloat3(&tangent, tn);

	binormal.x = f * (deltaUV1.x * edge2.x - deltaUV2.x * edge1.x);
	binormal.y = f * (deltaUV1.x * edge2.y - deltaUV2.x * edge1.y);
	binormal.z = f * (deltaUV1.x * edge2.z - deltaUV2.x * edge1.z);

	tn = XMLoadFloat3(&binormal);
	tn = XMVector3Normalize(tn);
	XMStoreFloat3(&binormal, tn);


	XMVECTOR vv0 = XMLoadFloat3(&v0.Pos);
	XMVECTOR vv1 = XMLoadFloat3(&v1.Pos);
	XMVECTOR vv2 = XMLoadFloat3(&v2.Pos);

	XMVECTOR e0 = vv1 - vv0;
	XMVECTOR e1 = vv2 - vv0;

	XMVECTOR e01cross = XMVector3Cross(e0, e1);
	e01cross = XMVector3Normalize(e01cross);
	XMFLOAT3 normalOut;
	XMStoreFloat3(&normalOut, e01cross);
	normal = normalOut;
}

void CalculateModelVectors(SimpleVertex* vertices, int vertexCount)
{
	int faceCount, i, index;
	SimpleVertex vertex1, vertex2, vertex3;
	XMFLOAT3 tangent, binormal, normal;


	// Calculate the number of faces in the model.
	faceCount = vertexCount / 3;

	// Initialize the index to the model data.
	index = 0;

	// Go through all the faces and calculate the the tangent, binormal, and normal vectors.
	for (i = 0; i < faceCount; i++)
	{
		// Get the three vertices for this face from the model.
		vertex1.Pos.x = vertices[index].Pos.x;
		vertex1.Pos.y = vertices[index].Pos.y;
		vertex1.Pos.z = vertices[index].Pos.z;
		vertex1.TexCoord.x = vertices[index].TexCoord.x;
		vertex1.TexCoord.y = vertices[index].TexCoord.y;
		vertex1.Normal.x = vertices[index].Normal.x;
		vertex1.Normal.y = vertices[index].Normal.y;
		vertex1.Normal.z = vertices[index].Normal.z;
		index++;

		vertex2.Pos.x = vertices[index].Pos.x;
		vertex2.Pos.y = vertices[index].Pos.y;
		vertex2.Pos.z = vertices[index].Pos.z;
		vertex2.TexCoord.x = vertices[index].TexCoord.x;
		vertex2.TexCoord.y = vertices[index].TexCoord.y;
		vertex2.Normal.x = vertices[index].Normal.x;
		vertex2.Normal.y = vertices[index].Normal.y;
		vertex2.Normal.z = vertices[index].Normal.z;
		index++;

		vertex3.Pos.x = vertices[index].Pos.x;
		vertex3.Pos.y = vertices[index].Pos.y;
		vertex3.Pos.z = vertices[index].Pos.z;
		vertex3.TexCoord.x = vertices[index].TexCoord.x;
		vertex3.TexCoord.y = vertices[index].TexCoord.y;
		vertex3.Normal.x = vertices[index].Normal.x;
		vertex3.Normal.y = vertices[index].Normal.y;
		vertex3.Normal.z = vertices[index].Normal.z;
		index++;

		// Calculate the tangent and binormal of that face.
		CalculateTangentBinormalLH(vertex1, vertex2, vertex3, normal, tangent, binormal);

		// Store the normal, tangent, and binormal for this face back in the model structure.
		vertices[index - 1].Normal.x = normal.x;
		vertices[index - 1].Normal.y = normal.y;
		vertices[index - 1].Normal.z = normal.z;
		vertices[index - 1].Tangent.x = tangent.x;
		vertices[index - 1].Tangent.y = tangent.y;
		vertices[index - 1].Tangent.z = tangent.z;
		vertices[index - 1].BiNormal.x = binormal.x;
		vertices[index - 1].BiNormal.y = binormal.y;
		vertices[index - 1].BiNormal.z = binormal.z;

		vertices[index - 2].Normal.x = normal.x;
		vertices[index - 2].Normal.y = normal.y;
		vertices[index - 2].Normal.z = normal.z;
		vertices[index - 2].Tangent.x = tangent.x;
		vertices[index - 2].Tangent.y = tangent.y;
		vertices[index - 2].Tangent.z = tangent.z;
		vertices[index - 2].BiNormal.x = binormal.x;
		vertices[index - 2].BiNormal.y = binormal.y;
		vertices[index - 2].BiNormal.z = binormal.z;

		vertices[index - 3].Normal.x = normal.x;
		vertices[index - 3].Normal.y = normal.y;
		vertices[index - 3].Normal.z = normal.z;
		vertices[index - 3].Tangent.x = tangent.x;
		vertices[index - 3].Tangent.y = tangent.y;
		vertices[index - 3].Tangent.z = tangent.z;
		vertices[index - 3].BiNormal.x = binormal.x;
		vertices[index - 3].BiNormal.y = binormal.y;
		vertices[index - 3].BiNormal.z = binormal.z;
	}

}

void CalculateTangentBinormalRH(SimpleVertex v0, SimpleVertex v1, SimpleVertex v2, XMFLOAT3& normal, XMFLOAT3& tangent, XMFLOAT3& binormal)
{
	// http://softimage.wiki.softimage.com/xsidocs/tex_tangents_binormals_AboutTangentsandBinormals.html

	// 1. CALCULATE THE NORMAL
	XMVECTOR vv0 = XMLoadFloat3(&v0.Pos);
	XMVECTOR vv1 = XMLoadFloat3(&v1.Pos);
	XMVECTOR vv2 = XMLoadFloat3(&v2.Pos);

	XMVECTOR P = vv1 - vv0;
	XMVECTOR Q = vv2 - vv0;

	XMVECTOR e01cross = XMVector3Cross(P, Q);
	XMFLOAT3 normalOut;
	XMStoreFloat3(&normalOut, e01cross);
	normal = normalOut;

	// 2. CALCULATE THE TANGENT from texture space

	float s1 = v1.TexCoord.x - v0.TexCoord.x;
	float t1 = v1.TexCoord.y - v0.TexCoord.y;
	float s2 = v2.TexCoord.x - v0.TexCoord.x;
	float t2 = v2.TexCoord.y - v0.TexCoord.y;


	float tmp = 0.0f;
	if (fabsf(s1 * t2 - s2 * t1) <= 0.0001f)
	{
		tmp = 1.0f;
	}
	else
	{
		tmp = 1.0f / (s1 * t2 - s2 * t1);
	}

	XMFLOAT3 PF3, QF3;
	XMStoreFloat3(&PF3, P);
	XMStoreFloat3(&QF3, Q);

	tangent.x = (t2 * PF3.x - t1 * QF3.x);
	tangent.y = (t2 * PF3.y - t1 * QF3.y);
	tangent.z = (t2 * PF3.z - t1 * QF3.z);

	tangent.x = tangent.x * tmp;
	tangent.y = tangent.y * tmp;
	tangent.z = tangent.z * tmp;

	XMVECTOR vn = XMLoadFloat3(&normal);
	XMVECTOR vt = XMLoadFloat3(&tangent);

	// 3. CALCULATE THE BINORMAL
	// left hand system b = t cross n (rh would be b = n cross t)
	XMVECTOR vb = XMVector3Cross(vt, vn);

	vn = XMVector3Normalize(vn);
	vt = XMVector3Normalize(vt);
	vb = XMVector3Normalize(vb);

	XMStoreFloat3(&normal, vn);
	XMStoreFloat3(&tangent, vt);
	XMStoreFloat3(&binormal, vb);

	return;
}

HRESULT DrawableGameObject::initMesh(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext)
{
	// Create vertex buffer
	SimpleVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },  
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, 

		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, 
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 

		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, 

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 

		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },

		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 

		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, 

		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, 

		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) , XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) }, 

		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) , XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) }, 

		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) }, 
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }, 

		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) }, 
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) }, 
	};

	CalculateModelVectors(vertices, 36);

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 36;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;
	HRESULT hr = pd3dDevice->CreateBuffer(&bd, &InitData, &m_pVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	pContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	// Create index buffer
	WORD indices[] =
	{
		0,1,2,
		3,4,5,

		6,7,8,
		9,10,11,

		12,13,14,
		15,16,17,

		18,19,20,
		21,22,23,

		24,25,26,
		27,28,29,

		30,31,32,
		33,34,35

	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * NUM_VERTICES;        // 36 vertices needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = pd3dDevice->CreateBuffer(&bd, &InitData, &m_pIndexBuffer);
	if (FAILED(hr))
		return hr;


	// Set index buffer
	pContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// load and setup textures
	hr = CreateDDSTextureFromFile(pd3dDevice, L"Resources\\color.dds", nullptr, &m_pTextureResourceView);
	if (FAILED(hr))
		return hr;

	hr = CreateDDSTextureFromFile(pd3dDevice, L"Resources\\normals.dds", nullptr, &m_pNormalResourceView);
	if (FAILED(hr))
		return hr;

	hr = CreateDDSTextureFromFile(pd3dDevice, L"Resources\\displacement.dds", nullptr, &m_pParraResourceView);
	if (FAILED(hr))
		return hr;

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
	m_material.Material.useNormals = true;
	m_material.Material.useParallax = true;

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

void DrawableGameObject::setPosition(XMFLOAT3 position)
{
	m_position = position;
}

void DrawableGameObject::update(float t, ID3D11DeviceContext* pContext)
{
	static float cummulativeTime = 0;
	cummulativeTime += t;

	// Cube:  Rotate around origin
	//XMMATRIX mSpin = XMMatrixRotationY(cummulativeTime);
	XMMATRIX mRotationX = XMMatrixRotationX(m_rotation.x);
	XMMATRIX mRotationY = XMMatrixRotationY(m_rotation.y);
	XMMATRIX mRotationZ = XMMatrixRotationZ(m_rotation.z);
	XMMATRIX mSpin = XMMatrixMultiply(XMMatrixRotationX(cummulativeTime), XMMatrixRotationY(cummulativeTime));
	XMMATRIX mTranslate = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX world = mTranslate;
	if (isSpinning == false)
	{
		 world = mRotationX * mRotationY * mRotationZ * mTranslate;
	}
	else if (isSpinning == true)
	{
		 world = mTranslate * mSpin;
	}
	XMStoreFloat4x4(&m_World, world);

	pContext->UpdateSubresource(m_pMaterialConstantBuffer, 0, nullptr, &m_material, 0, 0);
}

void DrawableGameObject::draw(ID3D11DeviceContext* pContext)
{
	pContext->PSSetShaderResources(1, 1, &m_pNormalResourceView);
	pContext->PSSetShaderResources(2, 1, &m_pParraResourceView);
	pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	pContext->DrawIndexed(NUM_VERTICES, 0, 0);
}
