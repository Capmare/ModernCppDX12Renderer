//
// Created by david on 10/28/2025.
//

#include "../../Header/ResourceManagement/DeviceContext.h"

namespace HOX {
    DeviceContext::DeviceContext() {
        m_Cleaner = std::make_unique<HOX::Cleaner>();
    }

    DeviceContext & GetDeviceContext() {
        static DeviceContext Instance; // This will get created only once
        return Instance;
    }
} // HOX