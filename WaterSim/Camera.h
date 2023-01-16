#pragma once
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include "resource.h"
#include "structures.h"

class Camera
{
public:
	Camera(XMFLOAT3 position, XMFLOAT3 at, XMFLOAT3 up, FLOAT windowWidth, FLOAT windowHeight, FLOAT nearDepth, FLOAT farDepth);
	~Camera();

	void Update();

	//Return Position, Lookat and up;
	XMFLOAT3 GetPosition() { return _eye; }
	void SetPosition(XMFLOAT3 position) { _eye = position; }

	XMFLOAT3 GetLookAt() { return _at; }
	void SetLookAt(XMFLOAT3 atPosition) { _at = atPosition; }

	XMFLOAT3 GetUp() { return _up; }
	void SetUp(XMFLOAT3 upPosition) { _up = upPosition; }

	//Return View and Projection
	XMFLOAT4X4* GetView() { return &_view; }
	void SetView();

	XMFLOAT4X4* GetProjection() { return &_projection; }
	void SetProjection();

	XMFLOAT4X4* GetOrthoProjection() { return &_projectionOrtho; }
	void SetOrthoProjection();


	void Reshape(FLOAT windowWidth, FLOAT windowHeight, FLOAT nearDepth, FLOAT farDepth);

private:
	XMFLOAT3 _eye;
	XMFLOAT3 _at;
	XMFLOAT3 _up;

	FLOAT _windowWidth;
	FLOAT _windowHeight;
	FLOAT _nearDepth;
	FLOAT _farDepth;

	XMFLOAT4X4 _view;
	XMFLOAT4X4 _projection;
	XMFLOAT4X4 _projectionOrtho;
};

