// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

static void
d3d11_init(void)
{
    HRESULT hr = S_OK;

    ID3D11Device *base_device = NULL;
    ID3D11DeviceContext *base_device_ctx = NULL;

    // Creation flags.
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if BUILD_DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL desired_levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL actual_level;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, desired_levels, array_count(desired_levels), D3D11_SDK_VERSION, &base_device, &actual_level, &base_device_ctx);
    assume(SUCCEEDED(hr));

    // @Todo: What if actual level isn't desired?

    hr = base_device->QueryInterface(__uuidof(d3d11.device), (void **)&d3d11.device);
    assume(SUCCEEDED(hr));

    base_device_ctx->QueryInterface(__uuidof(d3d11.device_ctx), (void **)&d3d11.device_ctx);

    // @Note: Set up debug layer to break on d3d11 errors.
#if BUILD_DEBUG
    ID3D11Debug *debug = NULL;
    ID3D11InfoQueue *d3d11_info = NULL;

    d3d11.device->QueryInterface(__uuidof(debug), (void **)&debug);
    assume(debug);

    hr = debug->QueryInterface(__uuidof(d3d11_info), (void **)&d3d11_info);
    assume(SUCCEEDED(hr));

    d3d11_info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
    d3d11_info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
    d3d11_info->Release();
#endif

    debug->Release();
    base_device->Release();
}

static void
d3d11_create_swapchain_and_framebuffer(HWND hwnd)
{
    HRESULT hr = S_OK;

    IDXGIFactory2 *dxgi_factory = NULL;
    IDXGIAdapter  *dxgi_adapter = NULL;
    DXGI_ADAPTER_DESC adapter_desc = {};


    if (d3d11.swapchain)
    { d3d11.swapchain->Release(); }

    if (d3d11.framebuffer_view)
    { d3d11.framebuffer_view->Release(); }

    hr = d3d11.device->QueryInterface(__uuidof(d3d11.dxgi_device), (void**)&d3d11.dxgi_device);
    assume(SUCCEEDED(hr));

    hr = d3d11.dxgi_device->GetAdapter(&dxgi_adapter);
    assume(SUCCEEDED(hr));

    dxgi_adapter->GetDesc(&adapter_desc);

    hr = dxgi_adapter->GetParent(__uuidof(dxgi_factory), (void**)&dxgi_factory);
    assume(SUCCEEDED(hr));

    DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
    {
        swapchain_desc.Width              = 0; // Use window width and height.
        swapchain_desc.Height             = 0;
        swapchain_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        swapchain_desc.Stereo             = FALSE;
        swapchain_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount        = 2;
        swapchain_desc.Scaling            = DXGI_SCALING_STRETCH;
        swapchain_desc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
        swapchain_desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapchain_desc.Flags              = 0;

        // @Note: No multisample for now.
        swapchain_desc.SampleDesc.Count   = 1;
        swapchain_desc.SampleDesc.Quality = 0;
    }

    hr = dxgi_factory->CreateSwapChainForHwnd(d3d11.device, hwnd, &swapchain_desc, NULL, NULL, &d3d11.swapchain);
    assume(SUCCEEDED(hr));

    // Create framebuffer render target
    ID3D11Texture2D *framebuffer;

    hr = d3d11.swapchain->GetBuffer(0, __uuidof(framebuffer), (void **)&framebuffer);
    assume(SUCCEEDED(hr));

    hr = d3d11.device->CreateRenderTargetView(framebuffer, 0, &d3d11.framebuffer_view);
    assume(SUCCEEDED(hr));

    // Cleanup
    framebuffer->Release();
    dxgi_factory->Release();
    dxgi_adapter->Release();
}
