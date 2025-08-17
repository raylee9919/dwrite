// Copyright (c) 2025 Seong Woo Lee. All rights reserved.


#ifndef LSW_D3D11_H
#define LSW_D3D11_H

typedef struct D3D11 D3D11;
struct D3D11
{
    ID3D11Device1 *device;
    ID3D11DeviceContext1 *device_ctx;

    IDXGISwapChain1 *swapchain;
    ID3D11RenderTargetView *framebuffer_view;
};

static D3D11 d3d11;

static void d3d11_init(D3D11 *d3d11);
static void d3d11_create_swapchain_and_framebuffer(HWND hwnd);

#endif // LSW_D3D11_H
