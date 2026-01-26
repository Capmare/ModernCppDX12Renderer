//
// Created by capma on 16-Nov-25.
//

module;
#include <d3d12.h>


module HOX.CommandSystem;

import HOX.Context;
import HOX.Logger;
import HOX.Context;

namespace HOX {
    void CommandSystem::Initialize() {
        GetDeviceContext().m_CommandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

    }

    ComPtr<ID3D12CommandQueue> CommandSystem::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12CommandQueue> CommandQueue{};

        D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
        QueueDesc.Type = Type;
        QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        QueueDesc.NodeMask = 0;

        HRESULT Hr = GetDeviceContext().m_Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(CommandQueue.ReleaseAndGetAddressOf()));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command queue.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command queue.");
        }


        return CommandQueue;
    }

    ComPtr<ID3D12CommandAllocator> CommandSystem::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12CommandAllocator> CommandAllocator{};

        HRESULT Hr = GetDeviceContext().m_Device->CreateCommandAllocator(Type, IID_PPV_ARGS(CommandAllocator.ReleaseAndGetAddressOf()));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command allocator.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command allocator.");
        }

        return CommandAllocator;
    }

    ComPtr<ID3D12GraphicsCommandList7> CommandSystem::CreateCommandList(ComPtr<ID3D12Device10> Device,
                                                                        ComPtr<ID3D12CommandAllocator> CommandAllocator,
                                                                        D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12GraphicsCommandList7> CommandList{};

        HRESULT Hr = Device->CreateCommandList(0, Type, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(CommandList.ReleaseAndGetAddressOf()));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command list.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command list.");
        }

        Hr = CommandList->Close();
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to close command list.");
        }

        return CommandList;
    }

    void CommandSystem::FlushCommands(ComPtr<ID3D12Fence> Fence,
                                      uint64_t &FenceValue, HANDLE FenceEvent) {
        uint64_t FenceValueForSignal{Signal(Fence, FenceValue)};
        WaitForFenceValues(Fence, FenceValueForSignal, FenceEvent);
    }

    uint64_t CommandSystem::Signal(ComPtr<ID3D12Fence> Fence,
                                   uint64_t& FenceValue) {
        uint64_t SignalValue = ++FenceValue;
        HRESULT Hr = GetDeviceContext().m_CommandQueue->Signal(Fence.Get(), SignalValue);

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to signal fence.");
        }

        return SignalValue;
    }

    void CommandSystem::WaitForFenceValues(ComPtr<ID3D12Fence> Fence, uint64_t FenceValue, HANDLE FenceEvent) {
        if (Fence->GetCompletedValue() < FenceValue) {
            HRESULT Hr = Fence->SetEventOnCompletion(FenceValue, FenceEvent);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to set fence event.");
            }

            WaitForSingleObject(FenceEvent, INFINITE);
        }
    }
} // HOX
