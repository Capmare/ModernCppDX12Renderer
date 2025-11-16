//
// Created by capma on 16-Nov-25.
//

#include "../../Header/Commands/CommandSystem.h"

namespace HOX {
    void CommandSystem::Initialize(const std::unique_ptr<Context> &Context) {
        Context->m_CommandQueue = CreateCommandQueue(Context->m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    ComPtr<ID3D12CommandQueue> CommandSystem::CreateCommandQueue(ComPtr<ID3D12Device10> Device,
                                                                 D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12CommandQueue> CommandQueue{};

        D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
        QueueDesc.Type = Type;
        QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        QueueDesc.NodeMask = 0;

        HRESULT Hr = Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command queue.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command queue.");
        }

        return CommandQueue;
    }

    ComPtr<ID3D12CommandAllocator> CommandSystem::CreateCommandAllocator(ComPtr<ID3D12Device10> Device,
                                                                         D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12CommandAllocator> CommandAllocator{};

        HRESULT Hr = Device->CreateCommandAllocator(Type, IID_PPV_ARGS(&CommandAllocator));
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

        HRESULT Hr = Device->CreateCommandList(0, Type, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
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

    void CommandSystem::FlushCommands(const std::unique_ptr<Context> &Context, ComPtr<ID3D12Fence> Fence,
                                      uint64_t &FenceValue, HANDLE FenceEvent) {
        uint64_t FenceValueForSignal{Signal(Context, Fence, FenceValue)};
        WaitForFenceValues(Fence, FenceValueForSignal, FenceEvent);
    }

    uint64_t CommandSystem::Signal(const std::unique_ptr<Context> &Context, ComPtr<ID3D12Fence> Fence,
                                   uint64_t FenceValue) {
        uint64_t SignalValue = ++FenceValue;
        HRESULT Hr = Context->m_CommandQueue->Signal(Fence.Get(), SignalValue);

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to signal fence.");
        } else {
            Logger::LogMessage(Severity::Info, "Signaling fence.");
        }

        return SignalValue;
    }

    void CommandSystem::WaitForFenceValues(ComPtr<ID3D12Fence> Fence, uint64_t FenceValue, HANDLE FenceEvent) {
        if (Fence->GetCompletedValue() < FenceValue) {
            HRESULT Hr = Fence->SetEventOnCompletion(FenceValue, FenceEvent);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to set fence event.");
            } else {
                Logger::LogMessage(Severity::Info, "Setting fence event.");
            }

            WaitForSingleObject(FenceEvent, INFINITE);
        }
    }
} // HOX
