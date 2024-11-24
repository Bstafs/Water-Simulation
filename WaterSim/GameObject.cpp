#include "GameObject.h"

GameObject::GameObject(string type, Geometry geometry, Material material)
{
	m_type = type;
	m_transform = new Transform();
	m_appearance = new Appearance(type, geometry, material);
}

GameObject::~GameObject()
{
}

void GameObject::Update(const float t)
{
	m_transform->Update(t);
}

void GameObject::DrawIndexed(ID3D11DeviceContext* pImmediateContext)
{
	m_appearance->DrawIndexed(pImmediateContext);
}

void GameObject::DrawInstanced(ID3D11DeviceContext* pImmediateContext)
{
	m_appearance->DrawInstanced(pImmediateContext);
}
