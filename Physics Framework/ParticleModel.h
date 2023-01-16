#pragma once
#include "Vector3.h"
#include "Transform.h"
#include <directxmath.h>
#include <d3d11_1.h>
#include "Debug.h"
using namespace DirectX;
using namespace std;
class ParticleModel
{
public:
	ParticleModel();
	~ParticleModel();

	void SetTransform(Transform* tf) { m_transform = tf; }
	Transform* GetTransform() { return m_transform; }

	void Update(const float deltaTime);

	void MoveConstantVelocity(const float deltaTime);
	void MoveConstantAcceleration(const float deltaTime);
	void Acceleration();

	// Setters and Getters for forces/collisions
	Vector3 GetVelocity() const { return m_velocity; }
	void SetVelocity(Vector3 velocity) { m_velocity = velocity; }
	void SetVelocity(float x, float y, float z) { m_velocity.x = x; m_velocity.y = y; m_velocity.z = z; }

	Vector3 GetAcceleration() const { return m_acceleration; }
	void SetAcceleration(Vector3 acceleration) { m_acceleration = acceleration; }
	void SetAcceleration(float x, float y, float z) { m_acceleration.x = x; m_acceleration.y = y; m_acceleration.z = z; }

	Vector3 GetNetForce() const { return m_netForce; }
	void SetNetForce(Vector3 netForce) { m_netForce = netForce; }
	void SetNetForce(float x, float y, float z) { m_netForce.x = x; m_netForce.y = y; m_netForce.z = z; }

	float GetMass() const { return m_mass; }
	void SetMass(float mass) { m_mass = mass; }

	Vector3 GetDrag() const { return m_drag; }
	void SetDrag(Vector3 drag) { m_drag = drag; }
	void SetDrag(float x, float y, float z) { m_drag.x = x; m_drag.y = y; m_drag.z = z; }

	Vector3 GetFriction() const { return m_friction; }
	void SetFriction(Vector3 friction) { m_friction = friction; }
	void SetFriction(float x, float y, float z) { m_friction.x = x; m_friction.y = y; m_friction.z = z; }

	bool ToggleGravity() const { return m_toggleGravity; }
	void SetToggleGravity(bool gravity){ m_toggleGravity = gravity; }

	void AddForce(Vector3 force) { m_netForce += force; }

	// Collisions
	float GetCollisionRadius() const { return m_boundSphereRadius; }
	void SetCollisionRadius(float radius) { m_boundSphereRadius = radius; }

	bool CheckSphereColision(Vector3 position, float radius);
	bool CheckCubeCollision(Vector3 position, float radius);

private:
	void Gravity();
	void Friction();
	void DragForce();
	void DragLaminarFlow();
	void DragTurbulentFlow();
	void Thrust(float deltaTime);
	void UpdatePosition(const float deltaTime);
	void CheckLevel();
protected:
	Vector3 m_netForce;
	Vector3 m_velocity;
	Vector3 m_acceleration;
	Vector3 m_friction;
	Vector3 m_drag;
	Vector3 m_thrust;

	Transform* m_transform;

	float m_mass = 1.0f;

	bool m_useLaminar;
private:
	float m_gravity;
	float m_weight = 1.0f;

	bool m_hasGravity;
	bool m_toggleGravity = true;

	float m_boundSphereRadius;

};

