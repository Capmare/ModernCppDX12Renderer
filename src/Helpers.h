//
// Created by david on 10/30/2025.
//

#ifndef MODERNCPPDX12RENDERER_HELPERS_H
#define MODERNCPPDX12RENDERER_HELPERS_H

namespace HOX {
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::exception();
        }
    }
}

#endif //MODERNCPPDX12RENDERER_HELPERS_H