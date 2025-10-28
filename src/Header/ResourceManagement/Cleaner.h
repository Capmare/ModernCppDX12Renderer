//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_HOXCLEANER_H
#define MODERNCPPDX12RENDERER_HOXCLEANER_H
#include <functional>
#include <vector>


namespace HOX {
    class Cleaner {

    public:
        Cleaner() = default;
        ~Cleaner() = default;

        // Prevent copy and move
        Cleaner(const Cleaner&) = delete;
        Cleaner& operator=(const Cleaner&) = delete;
        Cleaner(Cleaner&&) = delete;
        Cleaner& operator=(Cleaner&&) = delete;

        void AddToCleaner(const std::function<void()>& Func);
        void Clean();
    private:
        std::vector<std::function<void()>> m_Cleaner{};

    };


}


#endif //MODERNCPPDX12RENDERER_HOXCLEANER_H