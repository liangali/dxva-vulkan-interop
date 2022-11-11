#pragma once
#include <string>
#include <vector>
#include <map>

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <dxva.h>
#include <d3d11.h>
#include <dxgi1_6.h>

#define CHECK_FAIL_EXIT(hr, msg) \
    if (!SUCCEEDED(hr)) { printf("ERROR: Failed to call %s\n exit", msg); exit(-1); }

#define FREE_RESOURCE(res) \
    if(res) {res->Release(); res = NULL;}

typedef struct DXVADecBuf {
    D3D11_VIDEO_DECODER_BUFFER_TYPE bufType;
    uint8_t* pBufData;
    uint32_t bufSize;
} DXVADecBuf;

typedef struct _DXVAData {
    GUID guidDecoder;
    uint32_t picWidth;
    uint32_t picHeight;
    uint32_t isShortFormat;
    uint32_t dxvaBufNum;
    DXVADecBuf dxvaDecBuffers[10];
} DXVAData;

class D3d11Dxva
{
public:
    D3d11Dxva();
    D3d11Dxva(uint32_t w, uint32_t h, DXGI_FORMAT fmt= DXGI_FORMAT_B8G8R8A8_UNORM);
    ~D3d11Dxva();

    int init();
    int execute();
    int destory();

    ID3D11Texture2D* getTexture2D();
    IDXGIResource1* getDXGIResource();
    HANDLE getSharedHandle();

    ID3D11Device* d3d11Device() {
        return pD3D11Device_;
    };

    ID3D11DeviceContext* d3d11DeviceContext() {
        return pDeviceContext_;
    };

private:
    void printProfiles();
    void queryFormats();

    int decodeFrame();
    int processFrame();
    int copySurface();

    int getDecTexture2D(ID3D11Texture2D* & tex, uint32_t& w, uint32_t& h, DXGI_FORMAT& fmt);
    int getVppTexture2D(ID3D11Texture2D* & tex, uint32_t& w, uint32_t& h, DXGI_FORMAT& fmt);
    int dumpSurfaceToFile(ID3D11Texture2D* surf, uint32_t w, uint32_t h, DXGI_FORMAT fmt);

private:
    uint32_t shortFormat_ = 0;
    GUID profile_ = {};
    DXVAData dxvaDecData_ = {};

    uint32_t decWidth_ = 0;
    uint32_t decHeight_ = 0;
    DXGI_FORMAT decFormat_ = DXGI_FORMAT_UNKNOWN;

    uint32_t vppWidth_ = 0;
    uint32_t vppHeight_ = 0;
    DXGI_FORMAT vppFormat_ = DXGI_FORMAT_UNKNOWN;

    ID3D11Device *pD3D11Device_ = nullptr;
    ID3D11DeviceContext *pDeviceContext_ = nullptr;
    ID3D11VideoContext* pVideoContext_ = nullptr;
    ID3D11VideoDevice * pVideoDevice_ = nullptr;
    ID3D11VideoDecoder *pVideoDecoder_ = nullptr;

    ID3D11Texture2D *pSurfaceDecodeNV12_ = nullptr;
    ID3D11VideoDecoderOutputView *pDecodeOutputView_ = nullptr;

    ID3D11VideoProcessor *pVideoProcessor_ = nullptr;
    ID3D11VideoProcessorEnumerator *pVideoProcessorEnumerator_ = nullptr;
    ID3D11Texture2D * pSurfaceVppOut_ = nullptr;
    ID3D11VideoProcessorInputView *pVppInputView_ = nullptr;
    ID3D11VideoProcessorOutputView *pVppOutputView_ = nullptr;

    ID3D11Texture2D * pSurfaceCopyDst_ = nullptr;
    bool executed_ = false;
};
