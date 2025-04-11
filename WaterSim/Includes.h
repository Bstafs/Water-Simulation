#pragma once

#define NUM_OF_PARTICLES 4096
#define SMOOTHING_RADIUS 2.001f

// DirectX
#include <windows.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "DDSTextureLoader.h"
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include "CreateID3D11Functions.h"
// Camera
#include "Camera.h"

// Constants
#include "resource.h"
#include "Structures.h"
#include "Constants.h"
// OBJ Loader
#include "OBJLoader.h"

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

// Physics / Objects
#include "Quaternion.h"
#include "GameObject.h"
#include "Vector3.h"

// ImGui
#include"imgui/imgui.h"
#include"imgui/imgui_impl_dx11.h"
#include"imgui/imgui_impl_win32.h"
