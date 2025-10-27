//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_BUILDER_H
#define MODERNCPPDX12RENDERER_BUILDER_H

#include <string>

namespace HOX {
    template<class ObjectType, class Derived>
    class Builder {
    public:
        Builder(const std::string& Name) : m_Name(Name) {};
        ~Builder() = default;

        // Prevent copy and move
        Builder(const Builder &) = delete;
        Builder &operator=(const Builder &) = delete;
        Builder(Builder &&) = delete;
        Builder &operator=(Builder &&) = delete;

        // Build the object class and return its type
        ObjectType Build() {
            return static_cast<Derived*>(this)->BuildImpl();
        }

        // not all objects can have their validity verified
        // virtual void VerifyValidity();

        std::string GetName() const { return m_Name; }
    private:
        const std::string m_Name{};




    };
} // HOX

#endif //MODERNCPPDX12RENDERER_BUILDER_H
