static const float XM_PI = 3.1415926;

// Smoothing kernel for density
float DensitySmoothingKernel(float distance, float h)
{
    float q = distance / h;
    if (q >= 0.0f && q <= 1.0f)
    {
        float factor = (315.0f / (64.0f * 3.14159f * pow(h, 9)));
        return factor * pow(h * h - distance * distance, 3);
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

// Smoothing kernel for near-density
float NearDensitySmoothingKernel(float distance, float h)
{
    float q = distance / h;
    if (q >= 0.0f && q <= 1.0f)
    {
        float factor = (15.0f / (3.14159f * pow(h, 6)));
        return factor * pow(h - distance, 3);
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