#include "CreateID3D11Functions.h"

ID3DBlob* tempBlob;

ID3D11VertexShader* CreateVertexShader(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11Device* device)
{
	HRESULT hr;
	ID3D11VertexShader* vertexShader = nullptr;

	// Compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;

	hr = CompileShaderFromFile(szFileName, szEntryPoint, szShaderModel, &pVSBlob);

	if (FAILED(hr))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
	}

	// Create the vertex shader
	hr = device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &vertexShader);

	tempBlob = pVSBlob;

	if (FAILED(hr))
	{
		pVSBlob->Release();
	}


	return vertexShader;
}

ID3D11PixelShader* CreatePixelShader(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11Device* device)
{
	HRESULT hr;
	ID3D11PixelShader* pixelShader = nullptr;

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(szFileName, szEntryPoint, szShaderModel, &pPSBlob);

	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
	}

	// Create the pixel shader
	hr = device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pixelShader);
	pPSBlob->Release();



	return pixelShader;
}

ID3D11ComputeShader* CreateComputeShader(const WCHAR* szFileName, LPCSTR szEntryPoint, ID3D11Device* device)
{
	HRESULT hr;
	ID3D11ComputeShader* computeShader = nullptr;

	// Compile Compute Shader
	ID3DBlob* csBlob = nullptr;
	hr = CompileComputeShader(szFileName, szEntryPoint, device, &csBlob);
	if (FAILED(hr))
	{
		device->Release();
	}

	// Create Compute Shader
	hr = device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &computeShader);
	csBlob->Release();

	if (FAILED(hr))
	{
		device->Release();
	}

	return computeShader;
}

ID3D11InputLayout* CreateInputLayout(D3D11_INPUT_ELEMENT_DESC inputDesc[], UINT numElements, ID3D11Device* device)
{
	HRESULT hr;
	ID3D11InputLayout* inputLayout;

	// Create the input layout
	hr = device->CreateInputLayout(inputDesc, numElements, tempBlob->GetBufferPointer(), tempBlob->GetBufferSize(), &inputLayout);
	tempBlob->Release();

	return inputLayout;
}

ID3D11Buffer* CreateConstantBuffer(int byteWidth, ID3D11Device* device, bool isCompute)
{
	D3D11_BUFFER_DESC bufferDesc;
	ID3D11Buffer* outputBuffer;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));

	if (isCompute)
	{
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = byteWidth;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
	}
	else
	{
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = byteWidth;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
	}

	HRESULT hr = device->CreateBuffer(&bufferDesc, nullptr, &outputBuffer);

	return outputBuffer;
}

ID3D11Buffer* CreateStructureBuffer(int byteWidth, float* data, int vertexCount, ID3D11Device* device)
{
	ID3D11Buffer* structureBuffer = nullptr;
	HRESULT hr;
	D3D11_BUFFER_DESC constantDataDesc;
	constantDataDesc.Usage = D3D11_USAGE_DEFAULT;
	constantDataDesc.ByteWidth = byteWidth * vertexCount;
	constantDataDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	constantDataDesc.CPUAccessFlags = 0;
	constantDataDesc.StructureByteStride = byteWidth;
	constantDataDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	if (data)
	{
		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = data;
		hr = device->CreateBuffer(&constantDataDesc, &InitData, &structureBuffer);
	}
	else
	{
		hr = device->CreateBuffer(&constantDataDesc, nullptr, &structureBuffer);
	}

	return structureBuffer;
}

ID3D11Buffer* CreateReadableStructureBuffer(int byteWidth, float* data, ID3D11Device* device)
{
	ID3D11Buffer* RWBuffer = nullptr;
	HRESULT hr;

	D3D11_BUFFER_DESC outputDesc;
	ZeroMemory(&outputDesc, sizeof(outputDesc));
	outputDesc.Usage = D3D11_USAGE_STAGING;
	outputDesc.ByteWidth = byteWidth;
	outputDesc.BindFlags = 0;
	outputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	if (data)
	{
		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = data;
		hr = device->CreateBuffer(&outputDesc, &InitData, &RWBuffer);
	}
	else
	{
		hr = device->CreateBuffer(&outputDesc, nullptr, &RWBuffer);
	}

	return RWBuffer;
}

void ComputeResourceBuffer(ID3D11Buffer** pBuffer, ID3D11UnorderedAccessView** uav,
	ID3D11ShaderResourceView** srv, int vertexCount, float* data, int byteWidth, ID3D11Device* device)
{
	*pBuffer = CreateStructureBuffer(byteWidth, data, vertexCount, device);
	*uav = CreateUnorderedAccessView(*pBuffer, vertexCount, device);
	*srv = CreateShaderResourceView(*pBuffer, vertexCount, device);

}

ID3D11ShaderResourceView* CreateShaderResourceView(ID3D11Buffer* pBuffer, int vertexCount, ID3D11Device* device)
{
	ID3D11ShaderResourceView* SRV = nullptr;
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	shaderResourceViewDesc.BufferEx.FirstElement = 0;
	shaderResourceViewDesc.BufferEx.Flags = 0;
	shaderResourceViewDesc.BufferEx.NumElements = vertexCount;
	HRESULT hr = device->CreateShaderResourceView(pBuffer, &shaderResourceViewDesc, &SRV);
	return SRV;
}

ID3D11UnorderedAccessView* CreateUnorderedAccessView(ID3D11Buffer* pBuffer, int vertexCount, ID3D11Device* device)
{
	ID3D11UnorderedAccessView* UAV = nullptr;
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = vertexCount;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	HRESULT hr = device->CreateUnorderedAccessView(pBuffer, &uavDesc, &UAV);
	return UAV;
}

void UpdateBuffer(float* data, int byteWidth, ID3D11Buffer* buffer, ID3D11DeviceContext* device)
{
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE mappedData;

	float* mappedPtr;

	unsigned int bufferNumber;

	hr = device->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
	if(FAILED(hr))
	{
		return;
	}

	mappedPtr = (float*)mappedData.pData;

	memcpy(mappedPtr, data, byteWidth);

	device->Unmap(buffer, 0);

}

void* MapBuffer(ID3D11Buffer* buffer, ID3D11DeviceContext* device)
{
	D3D11_MAPPED_SUBRESOURCE mappingData;
	device->Map(buffer, 0, D3D11_MAP_READ, 0, &mappingData);
	return mappingData.pData;
}

void UnMapBuffer(ID3D11Buffer* buffer, ID3D11DeviceContext* device)
{
	device->Unmap(buffer, 0);
}

void SetDebugName(ID3D11DeviceChild* object, const std::string& name)
{
	if (object != nullptr)
		object->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.c_str());
}

