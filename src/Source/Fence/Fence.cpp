//
// Created by david on 12/23/2025.
//

module;

module HOX.Fence;
import HOX.Win32;
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
        HRESULT Hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
        HOX::Win32::UuidOf<ID3D12Fence>(),
        HOX::Win32::PpvArgs(Fence.ReleaseAndGetAddressOf())
            );



        if (HOX::Win32::Failed(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create fence.");
        } else {
            Logger::LogMessage(Severity::Info, "Created fence.");
        }

        return Fence;

    }

    HANDLE Fence::CreateFenceEvent() {
        HANDLE FenceEvent = HOX::Win32::CreateEvent_(nullptr, false, false, nullptr);
        if (FenceEvent == nullptr) {
            Logger::LogMessage(Severity::Error, "Failed to create fence event.");
        } else {
            Logger::LogMessage(Severity::Info, "Created fence event.");
        }

        return FenceEvent;
    }

} // HOX