#include "Appearance.h"

Appearance::Appearance(string type, Geometry geometry, Material material) : _geometry(geometry), _type(type), _material(material)
{
	_textureRV = nullptr;
}
Appearance::~Appearance()
{

}

void Appearance::DrawIndexed(ID3D11DeviceContext* pImmediateContext)
{
	// NOTE: We are assuming that the constant buffers and all other draw setup has already taken place
	const UINT strides = _geometry.vertexBufferStride;
	const UINT offsets = 0;

	// Set vertex and index buffers
	pImmediateContext->IASetVertexBuffers(0, 1, &_geometry.vertexBuffer, &strides, &offsets);
	pImmediateContext->IASetIndexBuffer(_geometry.indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	pImmediateContext->DrawIndexed(_geometry.numberOfIndices, 0, 0);
}

void Appearance::DrawInstanced(ID3D11DeviceContext* pImmediateContext)
{
	const UINT strides[2] = { _geometry.vertexBufferStride, _geometry.instanceBufferStride };
	const UINT offsets[2] = { 0, 0 };

	ID3D11Buffer* buffers[2] = { _geometry.vertexBuffer, _geometry.instanceBuffer };

	pImmediateContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	pImmediateContext->IASetIndexBuffer(_geometry.indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Draw instanced
	pImmediateContext->DrawIndexedInstanced(_geometry.numberOfIndices, _geometry.numberOfInstances, 0, 0, 0);
}
