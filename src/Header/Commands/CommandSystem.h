//
// Created by capma on 16-Nov-25.
//

#ifndef COMMANDSYSTEM_H
#define COMMANDSYSTEM_H

#include "../../pch.h"
#include <memory>


namespace HOX {
    class Fence;

    class CommandSystem {
    public:
        CommandSystem() = default;
        virtual ~CommandSystem() = default;

        CommandSystem(const CommandSystem &) = delete;
        CommandSystem(CommandSystem &&) noexcept = delete;
        CommandSystem &operator=(const CommandSystem &) = delete;
        CommandSystem &operator=(CommandSystem &&) noexcept = delete;

        void Initialize();
        ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE Type);
        ComPtr<ID3D12GraphicsCommandList7> CreateCommandList(ComPtr<ID3D12Device10> Device,
                                                             ComPtr<ID3D12CommandAllocator> CommandAllocator,
                                                             D3D12_COMMAND_LIST_TYPE Type);

        uint64_t Signal(ComPtr<ID3D12Fence> Fence, uint64_t &FenceValue);

        void WaitForFenceValues(ComPtr<ID3D12Fence> Fence, uint64_t FenceValue, HANDLE FenceEvent);

        void FlushCommands(ComPtr<ID3D12Fence> Fence, uint64_t &FenceValue, HANDLE FenceEvent);

    private:
        ComPtr<ID3D12CommandQueue> CreateCommandQueue(D3D12_COMMAND_LIST_TYPE Type);

    };
} // HOX

#endif //COMMANDSYSTEM_H
