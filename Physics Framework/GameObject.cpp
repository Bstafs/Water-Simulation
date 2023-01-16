#include "GameObject.h"

GameObject::GameObject(string type, Geometry geometry, Material material)
{
	m_type = type;
	m_transform = new Transform();
	m_appearance = new Appearance(type, geometry, material);
	m_physics = new Physics();
	m_physics->SetTransform(m_transform);
	m_rigidbody = new RigidBody();
	m_rigidbody->SetTransform(m_transform);
}

GameObject::~GameObject()
{
}

void GameObject::Update(const float t)
{
	m_physics->Update(t);
	m_rigidbody->Update(t);
	m_transform->Update(t);
}

void GameObject::Draw(ID3D11DeviceContext* pImmediateContext)
{
	m_appearance->Draw(pImmediateContext);
}