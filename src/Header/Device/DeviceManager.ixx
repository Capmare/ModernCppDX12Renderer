//
// Created by capma on 16-Nov-25.
//

module;
#include <d3d12.h>
#include <dxgi1_6.h>

import HOX.Win32;

export module HOX.DeviceManager;

import std;

export namespace HOX {

    using HOX::Win32::ComPtr;

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

