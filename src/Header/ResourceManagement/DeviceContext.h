//
// Created by david on 10/28/2025.
//

#ifndef MODERNCPPDX12RENDERER_DEVICECONTEXT_H
#define MODERNCPPDX12RENDERER_DEVICECONTEXT_H
#include <memory>

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

        HOX::Cleaner* GetCleaner() const { return m_Cleaner.get(); }
    private:
        std::unique_ptr<HOX::Cleaner> m_Cleaner;
    };


    DeviceContext& GetDeviceContext();

} // HOX

#endif //MODERNCPPDX12RENDERER_DEVICECONTEXT_H
