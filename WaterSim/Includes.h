#pragma once

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

//constexpr UINT NUM_OF_PARTICLES = 256;
//constexpr UINT NUM_OF_PARTICLES = 4096;
//constexpr UINT NUM_OF_PARTICLES = 8192;
//constexpr UINT NUM_OF_PARTICLES = 16384;
constexpr UINT NUM_OF_PARTICLES = 131072;
//constexpr UINT NUM_OF_PARTICLES = 262144;
constexpr FLOAT SMOOTHING_RADIUS = 2.001f;
constexpr UINT THREADS_PER_GROUPs = 256;

constexpr int threadGroupCountX = (NUM_OF_PARTICLES + THREADS_PER_GROUPs - 1) / THREADS_PER_GROUPs;

constexpr UINT RADIX = 256; // For 8-bit digits
constexpr UINT RADIX_BITS = 8; // 8-bit digits
// DirectX
#include <windows.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "DDSTextureLoader.h"
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include "CreateID3D11Functions.h"
#include <wrl/client.h>
#include <DirectXCollision.h>

// Camera
#include "Camera.h"

// Constants
#include "resource.h"
#include "Structures.h"

// STD
#include <format>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <random>
#include <cstdlib> 
#include <ctime>   
#include <algorithm>
#include <thread>
#include <future>
#include <functional>
#include <mutex>
#include <array>
#include <memory>

// Physics / Objects
#include "Quaternion.h"
#include "Vector3.h"

// ImGui
#include"imgui/imgui.h"
#include"imgui/imgui_impl_dx11.h"
#include"imgui/imgui_impl_win32.h"

using Microsoft::WRL::ComPtr;