//
// Created by capma on 16-Nov-25.
//

#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <memory>
#include "../ResourceManagement/Context.h"

#include "../../pch.h"

namespace HOX {

class DeviceManager {

public:
    DeviceManager() = default;
    virtual ~DeviceManager() = default;

    DeviceManager(const DeviceManager&) = delete;
    DeviceManager(DeviceManager&&) noexcept = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    DeviceManager& operator=(DeviceManager&&) noexcept = delete;

    void Initialize();

    bool CheckTearingSupport();
    static void PrintDebugMessages(ID3D12Device *device);

private:
    void EnableDebugLayer();


    ComPtr<IDXGIAdapter4> QueryDx12Adapter();
    ComPtr<ID3D12Device10> CreateDevice();

    bool m_bUseWarp{};

};

} // HOX

#endif //DEVICEMANAGER_H
