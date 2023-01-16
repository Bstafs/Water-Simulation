#include "Transform.h"

Transform::Transform()
{
	XMVECTOR XMVectorToQuaternion = XMQuaternionRotationMatrix(XMMatrixRotationX(0.0f) * XMMatrixRotationX(0.0f) * XMMatrixRotationX(0.0f));
	XMFLOAT4 XMFloat4Thing;
	XMStoreFloat4(&XMFloat4Thing, XMVectorToQuaternion);

	m_position = Vector3();
	m_rotation = Quaternion(XMFloat4Thing.w, XMFloat4Thing.x, XMFloat4Thing.y, XMFloat4Thing.z);
	m_rotation.normalise();
	m_scale = Vector3(1.0f, 1.0f, 1.0f);
}

Transform::~Transform()
{

}

void Transform::Update(float t)
{

	// Calculate world matrix
	XMMATRIX scale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	XMMATRIX translation = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);

	XMMATRIX matrixRotation;
	CalculateTransformMatrixRowMajor(matrixRotation, Vector3(), m_rotation);

	XMStoreFloat4x4(&m_world, scale * matrixRotation * translation);
}
