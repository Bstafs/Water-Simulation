#pragma once

#include <PrimitiveBatch.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <VertexTypes.h>

#include <wrl/client.h>
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "PrimitiveBatch.h"
#include "ScreenGrab.h"
#include "VertexTypes.h"

void XM_CALLCONV DrawSphere(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch,
	const DirectX::BoundingSphere& sphere,
	DirectX::FXMVECTOR color = DirectX::Colors::White);

void XM_CALLCONV DrawRing(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch,
	DirectX::FXMVECTOR origin, DirectX::FXMVECTOR majorAxis, DirectX::FXMVECTOR minorAxis,
	DirectX::GXMVECTOR color = DirectX::Colors::White);
