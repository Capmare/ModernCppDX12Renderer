//
// Created by capma on 16-Nov-25.
//

module;

#include <d3d12.h>

export module HOX.CommandSystem;

import HOX.Types;
import HOX.Fence;

export namespace HOX {

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

        u64 Signal(ComPtr<ID3D12Fence> Fence, u64 &FenceValue);

        void WaitForFenceValues(ComPtr<ID3D12Fence> Fence, u64 FenceValue, HANDLE FenceEvent);

        void FlushCommands(ComPtr<ID3D12Fence> Fence, u64 &FenceValue, HANDLE FenceEvent);

    private:
        ComPtr<ID3D12CommandQueue> CreateCommandQueue(D3D12_COMMAND_LIST_TYPE Type);

    };
} // HOX

