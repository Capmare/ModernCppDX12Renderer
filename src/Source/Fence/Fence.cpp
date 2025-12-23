//
// Created by david on 12/23/2025.
//

#include "../../Header/Fence/Fence.h"

namespace HOX {
    Fence::Fence() {
        m_Fence = CreateFence(GetDeviceContext().m_Device);
        m_FenceEvent = CreateFenceEvent();

    }

    Fence::~Fence() {
    }

    ComPtr<ID3D12Fence> Fence::CreateFence(ComPtr<ID3D12Device2> Device) {

        ComPtr<ID3D12Fence> Fence{};
        HRESULT Hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
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