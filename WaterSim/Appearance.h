#pragma once
#include "Vector3.h"
#include <directxmath.h>
#include <d3d11_1.h>
#include <string>
#include <vector>

using namespace DirectX;

struct Geometry
{
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;
	ID3D11Buffer* instanceBuffer;
	int numberOfIndices;

	UINT vertexBufferStride;
	UINT vertexBufferOffset;

	UINT instanceBufferStride;
	UINT instanceBufferOffset;
};

struct Material
{
	XMFLOAT4 diffuse;
	XMFLOAT4 ambient;
	XMFLOAT4 specular;
	float specularPower;
};
class Appearance
{
public:
	Appearance(string type, Geometry geometry, Material material);
	~Appearance();

	Geometry GetGeometryData() const { return _geometry; }

	Material GetMaterial() const { return _material; }

	void SetTextureRV(ID3D11ShaderResourceView* textureRV) { _textureRV = textureRV; }
	ID3D11ShaderResourceView* GetTextureRV() const { return _textureRV; }
	bool HasTexture() const { return _textureRV ? true : false; }

	void Draw(ID3D11DeviceContext* pImmediateContext);
	void DrawInstanced(ID3D11DeviceContext* pImmediateContext, UINT instances);
private:

	ID3D11ShaderResourceView* _textureRV;
	Geometry _geometry;
	Material _material;
	string _type;
};

