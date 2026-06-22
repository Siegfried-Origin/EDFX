#include "FrameExport.h"

#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#undef min
#undef max
#include <tinyexr.h>


float FrameExport::DecodeR11G11B10Component(
    uint32_t mantissa,
    uint32_t exponent,
    uint32_t mantissaBits)
{
    if (exponent == 0)
    {
        if (mantissa == 0)
            return 0.0f;

        // denormal
        return ldexpf(
            (float)mantissa / (float)(1u << mantissaBits),
            -14);
    }

    if (exponent == 31)
    {
        return INFINITY;
    }

    float m = 1.0f +
        ((float)mantissa / (float)(1u << mantissaBits));

    return ldexpf(m, (int)exponent - 15);
}


void FrameExport::DecodeR11G11B10Float(
    uint32_t packed,
    float& r,
    float& g,
    float& b)
{
    uint32_t rm = packed & 0x3F;
    uint32_t re = (packed >> 6) & 0x1F;

    uint32_t gm = (packed >> 11) & 0x3F;
    uint32_t ge = (packed >> 17) & 0x1F;

    uint32_t bm = (packed >> 22) & 0x1F;
    uint32_t be = (packed >> 27) & 0x1F;

    r = DecodeR11G11B10Component(rm, re, 6);
    g = DecodeR11G11B10Component(gm, ge, 6);
    b = DecodeR11G11B10Component(bm, be, 5);
}


bool FrameExport::SaveR11G11B10TextureAsEXR(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* srcTex,
    const char* filename)
{
    D3D11_TEXTURE2D_DESC desc;
    srcTex->GetDesc(&desc);

    if (desc.Format != DXGI_FORMAT_R11G11B10_FLOAT)
        return false;

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;

    HRESULT hr = device->CreateTexture2D(
        &stagingDesc,
        nullptr,
        &staging);

    if (FAILED(hr))
        return false;

    context->CopyResource(staging, srcTex);

    D3D11_MAPPED_SUBRESOURCE mapped;

    hr = context->Map(
        staging,
        0,
        D3D11_MAP_READ,
        0,
        &mapped);

    if (FAILED(hr))
    {
        staging->Release();
        return false;
    }

    const int width = (int)desc.Width;
    const int height = (int)desc.Height;

    std::vector<float> images[3];

    images[0].resize(width * height);
    images[1].resize(width * height);
    images[2].resize(width * height);

    for (int y = 0; y < height; y++)
    {
        const uint32_t* row =
            (const uint32_t*)
            ((const uint8_t*)mapped.pData +
                y * mapped.RowPitch);

        for (int x = 0; x < width; x++)
        {
            float r, g, b;

            DecodeR11G11B10Float(
                row[x],
                r,
                g,
                b);

            int idx = y * width + x;

            images[0][idx] = b;
            images[1][idx] = g;
            images[2][idx] = r;
        }
    }

    context->Unmap(staging, 0);
    staging->Release();

    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = 3;

    float* image_ptr[3];
    image_ptr[0] = images[0].data(); // B
    image_ptr[1] = images[1].data(); // G
    image_ptr[2] = images[2].data(); // R

    image.images = (unsigned char**)image_ptr;
    image.width = width;
    image.height = height;

    header.num_channels = 3;

    header.channels =
        (EXRChannelInfo*)malloc(
            sizeof(EXRChannelInfo) * 3);

    strcpy(header.channels[0].name, "B");
    strcpy(header.channels[1].name, "G");
    strcpy(header.channels[2].name, "R");

    header.pixel_types =
        (int*)malloc(sizeof(int) * 3);

    header.requested_pixel_types =
        (int*)malloc(sizeof(int) * 3);

    for (int i = 0; i < 3; i++)
    {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
    }

    const char* err = nullptr;

    int ret = SaveEXRImageToFile(
        &image,
        &header,
        filename,
        &err);

    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);

    if (ret != TINYEXR_SUCCESS)
    {
        if (err)
        {
            //LogInfo("TinyEXR error: %s\n", err);
            FreeEXRErrorMessage(err);
        }
        return false;
    }

    return true;
}