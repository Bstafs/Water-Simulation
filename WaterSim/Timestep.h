#pragma once
#include <d3d11_1.h>

class Timestep
{
public:
    // Constructor to set custom fixed timestep
    Timestep(float targetFPS = 120.0f)
        : accumulatedTime(0.0f), m_deltaTime(0.0f), m_fixedTimeStep(1.0f / targetFPS),
        m_readyForPhysicsUpdate(false), previousTime(0)
    {
    }

    // Calculates time steps for both fixed updates and variable rendering
    void CalculateTimestep()
    {
        // Get the current time
        ULONGLONG currentTime = GetTickCount64();

        // Initialize previousTime on the first call
        if (previousTime == 0)
            previousTime = currentTime;

        // Calculate frame time in seconds
        float frameTime = (currentTime - previousTime) / 1000.0f;
        previousTime = currentTime;

        // Cap frame time to prevent instability during long frames
        if (frameTime > 0.25f) // Cap at 0.25 seconds (4 FPS)
            frameTime = 0.25f;

        // Accumulate time
        accumulatedTime += frameTime;

        // Fixed timestep update
        while (accumulatedTime >= m_fixedTimeStep)
        {
            accumulatedTime -= m_fixedTimeStep;
            m_readyForPhysicsUpdate = true;
        }

        // Store the remainder as delta time for rendering
        m_deltaTime = frameTime;
    }

    // Returns true if physics should be updated this frame
    bool IsReadyForPhysicsUpdate() const
    {
        return m_readyForPhysicsUpdate;
    }

    // Consumes the physics update flag
    void ConsumePhysicsUpdate()
    {
        m_readyForPhysicsUpdate = false;
    }

    // Returns the fixed time step duration
    float GetFixedTimeStep() const
    {
        return m_fixedTimeStep;
    }

    // Returns the variable time step duration for rendering
    float GetDeltaTime() const
    {
        return m_deltaTime;
    }

    // Sets a new target FPS (adjusts the fixed timestep)
    void SetTargetFPS(float targetFPS)
    {
        if (targetFPS > 0.0f)
            m_fixedTimeStep = 1.0f / targetFPS;
    }

private:
    float accumulatedTime;          // Accumulated time for fixed updates
    float m_deltaTime;              // Variable time step for rendering
    float m_fixedTimeStep;          // Fixed timestep duration
    bool m_readyForPhysicsUpdate;   // Flag for physics update readiness
    ULONGLONG previousTime;         // Tracks the last frame time
};

