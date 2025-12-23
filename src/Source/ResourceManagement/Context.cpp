//
// Created by david on 10/28/2025.
//

#include "../../Header/ResourceManagement/Context.h"

namespace HOX {
    Context::Context() {
        m_Cleaner = std::make_unique<HOX::Cleaner>();
        m_CommandSystem = std::make_unique<HOX::CommandSystem>();

    }

    Context & GetDeviceContext() {
        static Context Instance; // This will get created only once
        return Instance;
    }
} // HOX