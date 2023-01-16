#include "ParticleModel.h"
ParticleModel::ParticleModel()
{
	m_gravity = 9.81f;
	m_useLaminar = false;
}

ParticleModel::~ParticleModel()
{

}

void ParticleModel::Update(const float deltaTime)
{
	Gravity();
	Friction();
	DragForce();
	MoveConstantAcceleration(deltaTime);
	Acceleration();
	MoveConstantVelocity(deltaTime);
	Thrust(deltaTime);
	UpdatePosition(deltaTime);
	CheckLevel();

	m_netForce = Vector3(0, 0, 0);
	m_acceleration = Vector3(0, 0, 0);
}

void ParticleModel::MoveConstantAcceleration(const float deltaTime)
{
	// acceleration = acceleration * time + 0.5 * acceleration * time * time

	m_acceleration.x = m_acceleration.x * deltaTime + 0.5f * m_acceleration.x * deltaTime * deltaTime;
	m_acceleration.y = m_acceleration.y * deltaTime + 0.5f * m_acceleration.y * deltaTime * deltaTime;
	m_acceleration.z = m_acceleration.z * deltaTime + 0.5f * m_acceleration.z * deltaTime * deltaTime;
}
void ParticleModel::Acceleration()
{
	m_acceleration = m_netForce / m_mass;
}

void ParticleModel::MoveConstantVelocity(const float deltaTime)
{
	// velocity = old velocity + acceleration * time
	m_velocity = m_velocity + m_acceleration * deltaTime;
}

void ParticleModel::UpdatePosition(const float deltaTime)
{
	Vector3 m_position = m_transform->GetPosition();
	m_position.x += m_velocity.x * deltaTime;
	m_position.y += m_velocity.y * deltaTime;
	m_position.z += m_velocity.z * deltaTime;
	m_transform->SetPosition(m_position.x, m_position.y, m_position.z);
}

void ParticleModel::Friction()
{
	float frictionCoeffient = 0.05f;
	Vector3 unitVelocity = m_velocity.Normalize();

	m_friction.x = m_weight * frictionCoeffient;
	m_friction.z = m_weight * frictionCoeffient;

	m_netForce += (unitVelocity * m_friction.x) * -1.0f;
	m_netForce += (unitVelocity * m_friction.z) * -1.0f;
}

void ParticleModel::Thrust(float deltaTime)
{
	m_thrust.x = m_velocity.x * (m_mass / deltaTime);
	m_thrust.y = m_velocity.y * (m_mass / deltaTime);
	m_thrust.z = m_velocity.z * (m_mass / deltaTime);

	m_netForce += m_thrust;
}

void ParticleModel::Gravity()
{
	if (m_toggleGravity == true)
	{
		m_weight = m_mass * m_gravity;
		m_netForce.y -= m_weight;
	}
	else
	{
		return;
	}
}

void ParticleModel::DragForce()
{
	if (m_useLaminar == true)
	{
		DragLaminarFlow();
	}
	else
	{
		DragTurbulentFlow();
	}
}

void ParticleModel::DragLaminarFlow()
{
	float dragFactor = 0.8f;

	m_drag.x = (dragFactor * m_velocity.x) * -1.0f;
	m_drag.y = (dragFactor * m_velocity.y) * -1.0f;
	m_drag.z = (dragFactor * m_velocity.z) * -1.0f;

	m_netForce += m_drag;
}

void ParticleModel::DragTurbulentFlow()
{
	if (m_velocity.Magnitude() < 0.1f) return;
	float velocityMagnitude = m_velocity.Magnitude();
	Vector3 unitVelocity = m_velocity.Normalize();

	float fluidDensity = 1.225f;
	float referenceArea = 1.0f;
	float dragCoefficient = 1.05f; // cube drag coefficient 

	// 0.5 * fluid density * drag coefficient * reference area * velocity * velocity
	m_drag.x = 0.5f * fluidDensity * dragCoefficient * referenceArea * velocityMagnitude * velocityMagnitude;
	m_drag.y = 0.5f * fluidDensity * dragCoefficient * referenceArea * velocityMagnitude * velocityMagnitude;
	m_drag.z = 0.5f * fluidDensity * dragCoefficient * referenceArea * velocityMagnitude * velocityMagnitude;

	m_drag = (unitVelocity * m_drag.x) * -1.0f;
	m_drag = (unitVelocity * m_drag.y) * -1.0f;
	m_drag = (unitVelocity * m_drag.z) * -1.0f;


	m_netForce += m_drag;
}

bool ParticleModel::CheckSphereColision(Vector3 position, float radius)
{
	return ((m_transform->GetPosition().x - position.x) * (m_transform->GetPosition().x - position.x) +
		(m_transform->GetPosition().y - position.y) * (m_transform->GetPosition().y - position.y) +
		(m_transform->GetPosition().z - position.z) * (m_transform->GetPosition().z - position.z) <= m_boundSphereRadius * radius);
}

bool ParticleModel::CheckCubeCollision(Vector3 position, float radius)
{
	float cubeOffset = 0.5f; // cube offset for box collisions
	return ((m_transform->GetPosition().x - cubeOffset <= position.x + cubeOffset && m_transform->GetPosition().x + cubeOffset >= position.x - cubeOffset) &&
		(m_transform->GetPosition().y - cubeOffset <= position.y + cubeOffset && m_transform->GetPosition().y + cubeOffset >= position.y - cubeOffset) &&
		(m_transform->GetPosition().z - cubeOffset <= position.z + cubeOffset && m_transform->GetPosition().z + cubeOffset >= position.z - cubeOffset));
}

void ParticleModel::CheckLevel()
{
	Vector3 position = m_transform->GetPosition();

		if (position.y < 0.0f)
		{
				m_transform->SetPosition(position.x, 10.0f, position.z);
				m_velocity.y = 0.0f;
		}
}