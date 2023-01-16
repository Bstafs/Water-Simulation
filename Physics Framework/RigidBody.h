#pragma once
#include "ParticleModel.h"
#include <directxmath.h>
#include <d3d11_1.h>
#include "Quaternion.h"
#include "Vector3.h"
using namespace DirectX;

class RigidBody
{
public:
	RigidBody();
	~RigidBody();
	void Update(const float deltaTime);

	Vector3 GetAngularVelocity() const { return m_angularVelocity; }
	void SetAngularVelocity(Vector3 velocity) { m_angularVelocity = velocity; }
	void SetAngularVelocity(float x, float y, float z) { m_angularVelocity.x = x; m_angularVelocity.y = y; m_angularVelocity.z = z; }

	void SetTransform(Transform* tf) { m_transform = tf; }
	Transform* GetTransform() { return m_transform; }

	void CalculateTorque(Vector3 force, Vector3 forceAppliedLocation, const float deltaTime);

private:

	XMFLOAT3 m_angularAcceleration;

	XMFLOAT3X3 m_inertiaTensor;

	Vector3 m_torque;
	Vector3 m_angularVelocity;

	float m_angularDamping;

	Quaternion m_orientation;
	XMMATRIX orientationMatrix;

	Transform* m_transform;

	float m_mass = 1.0f;

private:
	void CalculateInertiaTensor(float dx, float dy, float dz);
	void CalculateAngularAcceleration();
	void CalculateAngularVelocity(const float deltaTime);
	void CalculateDrag(const float deltaTime);
	void CalculateAngularOrientation(const float deltaTime);
};

