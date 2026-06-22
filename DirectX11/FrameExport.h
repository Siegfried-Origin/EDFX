#pragma once

#include <cstdint>

#include <stb_image.h>
#include <stb_image_write.h>
#include <tinyexr.h>

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif


class FrameExport
{
public:
    static float DecodeR11G11B10Component(uint32_t mantissa, uint32_t exponent, uint32_t mantissaBits);
    static void DecodeR11G11B10Float(
        uint32_t packed,
        float& r,
        float& g,
        float& b);

    static bool SaveR11G11B10TextureAsEXR(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* srcTex,
        const char* filename);
};