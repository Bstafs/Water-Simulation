#pragma once
#include "CompileShader.h"


// Shaders
ID3D11VertexShader* CreateVertexShader(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11Device* device);
ID3D11PixelShader* CreatePixelShader(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11Device* device);
ID3D11ComputeShader* CreateComputeShader(const WCHAR* szFileName, LPCSTR szEntryPoint, ID3D11Device* device);

// Layouts
ID3D11InputLayout* CreateInputLayout(D3D11_INPUT_ELEMENT_DESC inputDesc[], UINT numElements, ID3D11Device* device);

// Buffers
ID3D11Buffer* CreateConstantBuffer(int byteWidth, ID3D11Device* device, bool isCompute);
ID3D11Buffer* CreateStructureBuffer(int byteWidth, float* data, int vertexCount, ID3D11Device* device);
ID3D11Buffer* CreateReadableStructureBuffer(int byteWidth, float* data, ID3D11Device* device);

// Resource Views
ID3D11ShaderResourceView* CreateShaderResourceView(ID3D11Buffer* pBuffer, int vertexCount, ID3D11Device* device);
ID3D11UnorderedAccessView* CreateUnorderedAccessView(ID3D11Buffer* pBuffer, int vertexCount, ID3D11Device* device);

// Compute Shader
void ComputeResourceBuffer(ID3D11Buffer** pBuffer, ID3D11UnorderedAccessView** uav, ID3D11ShaderResourceView** srv, int vertexCount, float* data, int byteWidth, ID3D11Device* device);

// Mapping
void UpdateBuffer(float* data, int byteWidth, ID3D11Buffer* buffer, ID3D11DeviceContext* device);
void* MapBuffer(ID3D11Buffer* buffer, ID3D11DeviceContext* device);
void UnMapBuffer(ID3D11Buffer* buffer, ID3D11DeviceContext* device);