#include "Camera.h"


Camera::Camera(XMFLOAT3 position, XMFLOAT3 at, XMFLOAT3 up, FLOAT windowWidth, FLOAT windowHeight, FLOAT nearDepth, FLOAT farDepth)
{
	_eye = position;
	_at = at;
	_up = up;

	_windowWidth = windowWidth;
	_windowHeight = windowHeight;
	_nearDepth = nearDepth;
	_farDepth = farDepth;
}

Camera::~Camera()
{
	
}

void Camera::Update()
{

}

void Camera::SetView()
{
	XMVECTOR Eye = XMVectorSet(_eye.x, _eye.y, _eye.z, 0.0f);
	XMVECTOR At = XMVectorSet(_at.x, _at.y, _at.z, 0.0f);
	XMVECTOR Up = XMVectorSet(_up.x, _up.y, _up.z, 0.0f);

	XMStoreFloat4x4(&_view, XMMatrixLookAtLH(Eye, At, Up));
}

void Camera::SetProjection()
{
	XMStoreFloat4x4(&_projection, XMMatrixPerspectiveFovLH(XM_PIDIV2, (FLOAT)_windowWidth / (FLOAT)_windowHeight, _nearDepth, _farDepth));
}

void Camera::SetOrthoProjection()
{
	XMStoreFloat4x4(&_projectionOrtho, XMMatrixOrthographicLH(_windowWidth, _windowHeight, _nearDepth, _farDepth));
}

void Camera::Reshape(FLOAT windowWidth, FLOAT windowHeight, FLOAT nearDepth, FLOAT farDepth)
{
	_windowHeight = windowHeight;
	_windowHeight = windowHeight;
	_nearDepth = nearDepth;
	_farDepth = farDepth;
}