//
// Created by david on 12/23/2025.
//

module;
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <combaseapi.h>
#include <Windows.h>

module HOX.Fence;
import HOX.Context;
import HOX.Logger;
import std;

namespace HOX {
    Fence::Fence() {
        m_Fence = CreateFence(GetDeviceContext().m_Device);
        m_FenceEvent = CreateFenceEvent();

    }

    Fence::~Fence() {
    }

    ComPtr<ID3D12Fence> Fence::CreateFence(ComPtr<ID3D12Device2> Device) {

        ComPtr<ID3D12Fence> Fence{};
        HRESULT Hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.ReleaseAndGetAddressOf()));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create fence.");
        } else {
            Logger::LogMessage(Severity::Info, "Created fence.");
        }

        return Fence;

    }

    HANDLE Fence::CreateFenceEvent() {
        HANDLE FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);\
        if (FenceEvent == nullptr) {
            Logger::LogMessage(Severity::Error, "Failed to create fence event.");
        } else {
            Logger::LogMessage(Severity::Info, "Created fence event.");
        }

        return FenceEvent;
    }

} // HOX