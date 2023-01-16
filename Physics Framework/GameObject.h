#pragma once

#include <directxmath.h>
#include <d3d11_1.h>
#include <string>
#include "Vector3.h"
#include "Transform.h"
#include "Appearance.h"
#include "ParticleModel.h"
#include "RigidBody.h"

using namespace DirectX;
using namespace std;

class GameObject
{
public:
	GameObject(string type, Geometry geometry, Material material);
	~GameObject();

	string GetType() const { return m_type; }

	Transform* GetTransform()const { return m_transform; }
	Appearance* GetAppearance()const { return m_appearance; }
	ParticleModel* GetParticleModel()const { return m_particleModel; }
	RigidBody* GetRigidBody()const { return m_rigidbody; }

	void SetParent(GameObject* parent) { m_parent = parent; }

	void Update(const float t);
	void Draw(ID3D11DeviceContext* pImmediateContext);


private:
	string m_type;

	GameObject* m_parent;

	Transform* m_transform;
	Appearance* m_appearance;
	ParticleModel* m_particleModel;
	RigidBody* m_rigidbody;
};

