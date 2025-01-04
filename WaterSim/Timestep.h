#pragma once
#include <d3d11_1.h>

#include <chrono>

class Timestep
{
public:
    // Constructor to set custom fixed timestep
    Timestep(float targetFPS)
        : accumulatedTime(0.0f), m_deltaTime(0.0f), m_fixedTimeStep(1.0f / targetFPS),
          m_readyForPhysicsUpdate(false)
    {
        // Get the starting time using high_resolution_clock
        previousTime = std::chrono::high_resolution_clock::now();
    }

    // Calculates time steps for both fixed updates and variable rendering
    void CalculateTimestep()
    {
        // Get the current time
        auto currentTime = std::chrono::high_resolution_clock::now();

        // Calculate frame time in seconds
        std::chrono::duration<float> frameDuration = currentTime - previousTime;
        float frameTime = frameDuration.count();
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
    float accumulatedTime;            // Accumulated time for fixed updates
    float m_deltaTime;                // Variable time step for rendering
    float m_fixedTimeStep;            // Fixed timestep duration
    bool m_readyForPhysicsUpdate;     // Flag for physics update readiness
    std::chrono::time_point<std::chrono::high_resolution_clock> previousTime; // Tracks the last frame time
};