#pragma once
#include "Vector3.h"
#include <directxmath.h>
#include <d3d11_1.h>
#include "Quaternion.h"
using namespace DirectX;
class Transform
{
public:
	Transform();
	~Transform();
	void Update(float t);

	// Setters and Getters for position/rotation/scale
	void SetPosition(Vector3 position) { m_position = position; }
	void SetPosition(float x, float y, float z) { m_position.x = x; m_position.y = y; m_position.z = z; }

	Vector3 GetPosition() const { return m_position; }

	void SetScale(Vector3 scale) { m_scale = scale; }
	void SetScale(float x, float y, float z) { m_scale.x = x; m_scale.y = y; m_scale.z = z; }

	Vector3 GetScale() const { return m_scale; }

	void SetRotation(Quaternion quat) { m_rotation = quat; }
	void SetRotation(Vector3 rotation) { SetRotation(rotation.x,rotation.y,rotation.z); }
	void SetRotation(float x, float y, float z) 
	{
		XMVECTOR xmVector = XMQuaternionRotationMatrix(XMMatrixRotationX(x) * XMMatrixRotationY(y) * XMMatrixRotationZ(z));
		XMFLOAT4 quaternionFloat;
		XMStoreFloat4(&quaternionFloat, xmVector);
		m_rotation = Quaternion(quaternionFloat.w, quaternionFloat.x, quaternionFloat.y, quaternionFloat.z);
		m_rotation.normalise();
	}

	Quaternion GetRotation() const { return m_rotation; }

	XMMATRIX GetWorldMatrix() const { return XMLoadFloat4x4(&m_world); }

protected:

	Vector3 m_position;

private:
	Quaternion m_rotation;
	Vector3 m_scale;
	Vector3 _rotation;
	XMFLOAT4X4 m_world;
};

