#include "RigidBody.h"

RigidBody::RigidBody()
{

	m_angularVelocity = Vector3();
	m_angularAcceleration = XMFLOAT3();
	m_angularDamping = 0.9f;

	//Calculate Inertia Tensor
	CalculateInertiaTensor(1.0f, 1.0f, 1.0f);
}

RigidBody::~RigidBody()
{

}

void RigidBody::Update(const float deltaTime)
{
	//Calculate Drag
	CalculateDrag(deltaTime);

	//Calculate Angular Orientation
	CalculateAngularOrientation(deltaTime);
}

void RigidBody::CalculateTorque(Vector3 force, Vector3 forceAppliedLocation, const float deltaTime)
{
	m_torque = forceAppliedLocation.CrossProduct(force);

	//Calculate Angular Acceleration
	CalculateAngularAcceleration();

	//Calculate Angular Velocity
	CalculateAngularVelocity(deltaTime);
}

void RigidBody::CalculateInertiaTensor(float dx, float dy, float dz)
{
	m_inertiaTensor = XMFLOAT3X3();
	m_inertiaTensor._11 = (1.0f / 12.0f) * m_mass * (dy * dy + dz * dz); // change 1.0 to mass // _11 _12 _13
 	m_inertiaTensor._22 = (1.0f / 12.0f) * m_mass * (dx * dx + dz * dz);                       // _21 _22 _23
 	m_inertiaTensor._33 = (1.0f / 12.0f) * m_mass * (dx * dx + dy * dy);                       // _31 _32 _33
}

void RigidBody::CalculateAngularAcceleration()
{
	XMMATRIX inverted = XMMatrixInverse(nullptr, XMLoadFloat3x3(&m_inertiaTensor));
	XMFLOAT3 torque = m_torque.Vector3ToXMFLOAT3();
	XMStoreFloat3(&m_angularAcceleration, XMVector3Transform(XMLoadFloat3(&torque), inverted));
}

void RigidBody::CalculateAngularVelocity(const float deltaTime)
{
	m_angularVelocity.x = m_angularVelocity.x + m_angularAcceleration.x * deltaTime;
	m_angularVelocity.y = m_angularVelocity.y + m_angularAcceleration.y * deltaTime;
	m_angularVelocity.z = m_angularVelocity.z + m_angularAcceleration.z * deltaTime;
}

void RigidBody::CalculateDrag(const float deltaTime)
{
	m_angularVelocity *= powf(m_angularDamping, deltaTime);
}

void RigidBody::CalculateAngularOrientation(const float deltaTime)
{
	Quaternion orientation = GetTransform()->GetRotation();
	orientation.addScaledVector(m_angularVelocity, deltaTime);
	orientation.normalise();

	XMMATRIX orientationMatrix = XMMATRIX();
	CalculateTransformMatrixRowMajor(orientationMatrix, m_transform->GetPosition(), orientation);

	m_transform->SetRotation(orientation);
}