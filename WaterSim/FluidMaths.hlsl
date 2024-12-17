static const float XM_PI = 3.1415926;

// Density Force Kernel
float DensitySmoothingKernel(float dst, float radius)
{
    if (dst >= 0 && dst <= radius)
    {
        const float scale = 15.0f / (2.0f * XM_PI * pow(abs(radius), 5));
        float diff = radius - dst;
        return diff * diff * scale;
    }
    return 0.0f;
}

// Pressure Force Kernel
float PressureSmoothingKernel(float dst, float radius)
{
    if (dst >= 0 && dst <= radius)
    {
        const float scale = 15.0f / (pow(abs(radius), 5) * XM_PI);
        float diff = radius - dst;
        return -diff * scale;
    }
    return 0.0f;
}

float NearDensitySmoothingKernel(float dst, float radius)
{
    if (dst >= 0 && dst <= radius)
    {
        const float scale = 15.0f / (XM_PI * pow(abs(radius), 6));
        float diff = radius - dst;
        return diff * diff * diff * scale;
    }
    return 0.0f;
}

float NearDensitySmoothingKernelDerivative(float dst, float radius)
{
    if (dst >= 0 && dst <= radius)
    {
        const float scale = 45.0f / (pow(abs(radius), 6) * XM_PI);
        float diff = radius - dst;
        return -diff * diff * scale;
    }
    return 0.0f;
}

float ViscositySmoothingKernel(float dst, float radius)
{
    if (dst >= 0 && dst <= radius)
    {
        const float scale = 315.0f / (64 * XM_PI * pow(abs(radius), 9));
        float diff = radius * radius - dst * dst;
        return diff * diff * diff * scale;
    }
    return 0.0f;
}