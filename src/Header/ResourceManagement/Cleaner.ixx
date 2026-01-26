//
// Created by david on 10/27/2025.
//



// Engine imports
export module HOX.Cleaner;

// Other imports
import std;

export namespace HOX {
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


