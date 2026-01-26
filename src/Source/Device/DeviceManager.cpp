//
// Created by capma on 16-Nov-25.
//


module;
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <combaseapi.h>


module HOX.DeviceManager;

import HOX.Context;
import HOX.Logger;

namespace HOX {

    void DeviceManager::Initialize() {
        EnableDebugLayer();
        GetDeviceContext().m_Adapter = QueryDx12Adapter();
        GetDeviceContext().m_Device = CreateDevice();

    }

    void DeviceManager::EnableDebugLayer() {
#ifdef _DEBUG
        ComPtr<ID3D12Debug1> debugController;
        ComPtr<IDXGIInfoQueue> infoQueue;

        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.ReleaseAndGetAddressOf()))) &&
            SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(infoQueue.ReleaseAndGetAddressOf())))) {

            debugController->EnableDebugLayer();
            debugController->SetEnableGPUBasedValidation(TRUE);
            debugController->SetEnableSynchronizedCommandQueueValidation(TRUE);

            infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);

            HOX::Logger::LogMessage(HOX::Severity::Info, "D3D12 debug layer enabled.");
            return;
            }

        HOX::Logger::LogMessage(HOX::Severity::Error, "Failed to get D3D12 debug layer.");
#endif
    }


    void DeviceManager::PrintDebugMessages(ID3D12Device* device) {
#ifdef _DEBUG
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(infoQueue.ReleaseAndGetAddressOf()))))
            return;

        UINT64 numMessages = infoQueue->GetNumStoredMessages();
        for (UINT64 i = 0; i < numMessages; ++i) {
            SIZE_T messageLength = 0;
            infoQueue->GetMessage(i, nullptr, &messageLength);

            std::vector<char> messageData(messageLength);
            D3D12_MESSAGE* msg = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
            infoQueue->GetMessage(i, msg, &messageLength);

            HOX::Severity sev = HOX::Severity::Info;
            switch (msg->Severity) {
                case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                case D3D12_MESSAGE_SEVERITY_ERROR:
                    sev = HOX::Severity::ErrorNoCrash; // Use non-crashing error for logging
                    break;
                case D3D12_MESSAGE_SEVERITY_WARNING:
                    sev = HOX::Severity::Warning;
                    break;
                case D3D12_MESSAGE_SEVERITY_INFO:
                case D3D12_MESSAGE_SEVERITY_MESSAGE:
                    sev = HOX::Severity::Info;
                    break;
            }

            HOX::Logger::LogMessage(sev, std::string("[D3D12 DEBUG] ") + msg->pDescription);
        }

        infoQueue->ClearStoredMessages();
#endif
    }

    ComPtr<IDXGIAdapter4> DeviceManager::QueryDx12Adapter() {
        ComPtr<IDXGIFactory7> DXGIFactory{};
        UINT CreateFactoryFlags = 0;
#ifdef _DEBUG
        CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        if (FAILED(CreateDXGIFactory2(CreateFactoryFlags, IID_PPV_ARGS(DXGIFactory.ReleaseAndGetAddressOf())))) {
            Logger::LogMessage(Severity::Error, "Failed to create DXGIFactory.");
            return nullptr;
        } else {
            Logger::LogMessage(Severity::Info, "DXGIFactory created.");
        }

        ComPtr<IDXGIAdapter1> DXGIAdapter1{};
        ComPtr<IDXGIAdapter4> DXGIAdapter4{};

        if (m_bUseWarp) {
            HRESULT hr = DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(DXGIAdapter1.ReleaseAndGetAddressOf()));
            if (FAILED(hr)) {
                Logger::LogMessage(Severity::Error, "Failed to enumerate warp adapter.");
                return nullptr;
            } else {
                Logger::LogMessage(Severity::Info, "Warp adapter enumerated.");
            }

            hr = DXGIAdapter1.As(&DXGIAdapter4);
            if (FAILED(hr)) {
                Logger::LogMessage(Severity::Error, "Failed to get warp adapter.");
                return nullptr;
            } else {
                Logger::LogMessage(Severity::Info, "Warp adapter found.");
            }

            return DXGIAdapter4;
        } else {
            SIZE_T MaxDedicatedVideoMemory = 0;
            DXGIAdapter4 = nullptr;

            for (UINT Idx = 0; DXGIFactory->EnumAdapters1(Idx, &DXGIAdapter1) != DXGI_ERROR_NOT_FOUND; Idx++) {
                DXGI_ADAPTER_DESC1 DXGIAdapterDesc1{};
                DXGIAdapter1->GetDesc1(&DXGIAdapterDesc1);

                if ((DXGIAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    // Test if this adapter can create a D3D12 device
                    if (SUCCEEDED(
                        D3D12CreateDevice(DXGIAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr
                        ))) {
                        if (DXGIAdapterDesc1.DedicatedVideoMemory > MaxDedicatedVideoMemory) {
                            MaxDedicatedVideoMemory = DXGIAdapterDesc1.DedicatedVideoMemory;
                            HRESULT hr = DXGIAdapter1.As(&DXGIAdapter4);
                            if (FAILED(hr)) {
                                Logger::LogMessage(Severity::Error, "Failed to get IDXGIAdapter4 from IDXGIAdapter1.");
                            } else {
                                Logger::LogMessage(Severity::Info,
                                                   "Found suitable adapter with dedicated video memory.");
                            }
                        }
                    }
                }
            }

            if (!DXGIAdapter4) {
                // fallback to WARP if no suitable adapter found
                Logger::LogMessage(Severity::Warning, "No suitable hardware adapter found, falling back to WARP.");
                HRESULT hr = DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(DXGIAdapter1.ReleaseAndGetAddressOf()));
                if (FAILED(hr)) {
                    Logger::LogMessage(Severity::Error, "Failed to enumerate warp adapter as fallback.");
                    return nullptr;
                }
                hr = DXGIAdapter1.As(&DXGIAdapter4);
                if (FAILED(hr)) {
                    Logger::LogMessage(Severity::Error, "Failed to get WARP adapter as fallback.");
                    return nullptr;
                }
            }

            return DXGIAdapter4;
        }
    }

    ComPtr<ID3D12Device10> DeviceManager::CreateDevice() {
        ComPtr<ID3D12Device10> Device{};

        HRESULT Hr = D3D12CreateDevice(GetDeviceContext().m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2,IID_PPV_ARGS(Device.ReleaseAndGetAddressOf()));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create D3D12 device.");
        } else {
            Logger::LogMessage(Severity::Info, "D3D12 device created.");
        }

#ifdef _DEBUG
        ComPtr<ID3D12InfoQueue> InfoQueue;
        if (SUCCEEDED(Device.As(&InfoQueue))) {
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            // Suppress whole categories of messages
            //D3D12_MESSAGE_CATEGORY Categories[] = {};

            // Suppress messages based on their severity level
            D3D12_MESSAGE_SEVERITY Severities[] =
            {
                D3D12_MESSAGE_SEVERITY_INFO
            };

            // Suppress individual messages by their ID
            D3D12_MESSAGE_ID DenyIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                // This warning occurs when using capture frame while graphics debugging.
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE, // Same
            };

            D3D12_INFO_QUEUE_FILTER NewFilter = {};
            //NewFilter.DenyList.NumCategories = _countof(Categories);
            //NewFilter.DenyList.pCategoryList = Categories;
            NewFilter.DenyList.NumSeverities = _countof(Severities);
            NewFilter.DenyList.pSeverityList = Severities;
            NewFilter.DenyList.NumIDs = _countof(DenyIds);
            NewFilter.DenyList.pIDList = DenyIds;

            Hr = InfoQueue->PushStorageFilter(&NewFilter);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to push storage filter.");
            } else {
                Logger::LogMessage(Severity::Info, "D3D12 info queue pushed.");
            }
        }
#endif


        return Device;
    }

    bool DeviceManager::CheckTearingSupport() {
        bool TearingSupport = false;

        UINT CreateFactoryFlags = 0;

#ifdef _DEBUG
        CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ComPtr<IDXGIFactory7> Factory{};

        if (SUCCEEDED(CreateDXGIFactory2(CreateFactoryFlags,IID_PPV_ARGS(Factory.ReleaseAndGetAddressOf())))) {
            if (SUCCEEDED(
                Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &TearingSupport, sizeof(TearingSupport)
                ))) {
                return true;
                }
        }
        return false;
    }
} // HOX