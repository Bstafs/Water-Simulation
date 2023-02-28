#pragma once

// DirectX
#include <windows.h>
#include "CompileShader.h"
#include <directxmath.h>
#include <directxcolors.h>
#include "DDSTextureLoader.h"
#include "DebugDraw.h"
#include <DirectXColors.h>
#include <DirectXCollision.h>

// Camera
#include "Camera.h"

// Constants
#include "resource.h"
#include "Structures.h"
#include "Constants.h"
// OBJ Loader
#include "OBJLoader.h"

// STD
#include <vector>
#include <unordered_map>

// Physics / Objects
#include "Quaternion.h"
#include "GameObject.h"
#include "Vector3.h"

// ImGui
#include"imgui/imgui.h"
#include"imgui/imgui_impl_dx11.h"
#include"imgui/imgui_impl_win32.h"