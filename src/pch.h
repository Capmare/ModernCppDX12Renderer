//
// Created by david on 10/30/2025.
//

#ifndef MODERNCPPDX12RENDERER_PCH_H
#define MODERNCPPDX12RENDERER_PCH_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// DirectX12 specific includes
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>


// Usual includes
#include "../src/Header/Logger/Logger.h"
#include "../src/Header/Commands/CommandSystem.h"
#include "../src/Header/Fence/Fence.h"
#include "../src/Header/ResourceManagement/Context.h"


static constexpr uint8_t m_MaxFrames{3};


#endif //MODERNCPPDX12RENDERER_PCH_H