//
// Created by david on 10/27/2025.
//

#include "../../Header/ResourceManagement/Cleaner.h"

namespace HOX {
    void Cleaner::AddToCleaner(const std::function<void()> &Func) {
        m_Cleaner.emplace_back(std::move(Func));
    }

    void Cleaner::Clean() {
        for (auto it = m_Cleaner.rbegin(); it != m_Cleaner.rend(); ++it)
            (*it)();
        m_Cleaner.clear();
    }
}
