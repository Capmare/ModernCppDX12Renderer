//
// Created by david on 10/28/2025.
//

#ifndef MODERNCPPDX12RENDERER_DEVICECONTEXT_H
#define MODERNCPPDX12RENDERER_DEVICECONTEXT_H
#include <memory>
#include "../../pch.h"

#include "Cleaner.h"

namespace HOX {
    class DeviceContext {
    public:
        DeviceContext();
        ~DeviceContext() = default;

        // Prevent copy and move
        DeviceContext(const DeviceContext&) = delete;
        DeviceContext& operator=(const DeviceContext&) = delete;
        DeviceContext(DeviceContext&&) = delete;
        DeviceContext& operator=(DeviceContext&&) = delete;

        ComPtr<ID3D12Device10> m_Device{};
        ComPtr<IDXGIAdapter4> m_Adapter{};
        std::unique_ptr<HOX::Cleaner> m_Cleaner;

        bool m_bUseVSync{false};
        bool m_bTearingSupported{false};

    private:

    };


    DeviceContext& GetDeviceContext();

} // HOX

#endif //MODERNCPPDX12RENDERER_DEVICECONTEXT_H
