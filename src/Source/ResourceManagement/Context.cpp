//
// Created by david on 10/28/2025.
//
module HOX.Context;

import std;
import HOX.Cleaner;
import HOX.CommandSystem;

namespace HOX {
    Context::Context() {
        m_Cleaner = std::make_unique<HOX::Cleaner>();
        m_CommandSystem = std::make_unique<HOX::CommandSystem>();

    }

    Context::~Context() {
    }

    Context & GetDeviceContext() {
        static Context Instance; // This will get created only once
        return Instance;
    }
} // HOX