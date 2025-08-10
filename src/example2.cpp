// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

//
// This program draws names of the fonts in the system with DWrite and D2D.
//

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite.h>
#include <d2d1.h>

#include <stdio.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d2d1")

#define assert(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define assert_hr(exp) if (FAILED(exp)) { assert(!"HRESULT assertion failed"); }

BOOL use_dwrite_text_layout = true;

static LRESULT CALLBACK
WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;

        default: {
            result = DefWindowProcW(hWnd, message, wParam, lParam);
        }
    }
    return result;
}

int WINAPI
wWinMain(HINSTANCE hinst, HINSTANCE /*hprevinst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
    WNDCLASSW window_class = {};
    {
        window_class.style              = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
        window_class.lpfnWndProc        = WndProc;
        window_class.cbClsExtra         = 0;
        window_class.cbWndExtra         = 0;
        window_class.hInstance          = hinst;
        window_class.hIcon              = NULL;
        window_class.hCursor            = LoadCursor(hinst, IDC_ARROW);
        window_class.hbrBackground      = (HBRUSH)GetStockObject(BLACK_BRUSH);
        window_class.lpszMenuName       = NULL;
        window_class.lpszClassName      = L"ClassUnnamed";
    }

    HRESULT hr = S_OK;

    assert(RegisterClassW(&window_class));

    HWND hwnd = CreateWindowExW(0, window_class.lpszClassName,
                                L"DirectWrite",
                                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                NULL, NULL, hinst, NULL);
    assert(hwnd);


    IDWriteFactory* dwrite_factory;
    ID2D1Factory *d2d_factory;
    IDWriteTextFormat* text_format;

    assert_hr(hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory));
    assert_hr(hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),  (IUnknown **)&dwrite_factory));
    assert_hr(hr = dwrite_factory->CreateTextFormat(L"Arial", NULL/*fontCollection*/,
                                               DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                               10.0f * 96.0f/72.0f, L"en-us", &text_format));


    //wchar_t *text = L"This text is rendered with DirectWrite and Direct2D";
    UINT32 cursor = 0;
    wchar_t text[4096];
    memset(text, 0, sizeof(wchar_t)*4096);


    // Get the system font collection and family count.
    IDWriteFontCollection *font_collection = 0;
    assert_hr(hr = dwrite_factory->GetSystemFontCollection(&font_collection));
    UINT32 family_count = font_collection->GetFontFamilyCount();

    for (UINT32 i = 0; i < family_count; ++i)
    {
        IDWriteFontFamily *font_family = 0;
        assert_hr(hr = font_collection->GetFontFamily(i, &font_family));

        IDWriteLocalizedStrings *family_names = 0;
        assert_hr(hr = font_family->GetFamilyNames(&family_names));

        UINT32 index = 0;
        BOOL exists = false;

        wchar_t locale_name[LOCALE_NAME_MAX_LENGTH];

        // Get the default locale for this user.
        int default_local_success = GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH);

        // If the default locale is returned, find that locale name, otherwise use "en-us".
        if (default_local_success)
        { assert_hr(hr = family_names->FindLocaleName(locale_name, &index, &exists)); }

        // If the above find did not find a match, retry with US English.
        if (! exists)
        { assert_hr(hr = family_names->FindLocaleName(L"en-us", &index, &exists)); }

        // If the specifies locale doesn't exist, select the first on the list.
        if (! exists)
        { index = 0; }

        // Get the string length.
        UINT32 length = 0;
        assert_hr(hr = family_names->GetStringLength(index, &length));

        // Allocate a string big enough to hold the name.
        wchar_t *name = new wchar_t[length + 1];
        assert(name);

        // Get the family name.
        assert_hr(hr = family_names->GetString(index, name, length + 1));

        // Copy to the string we are about to render.
        for (UINT32 j = 0; j < length; ++j)
        { text[cursor + j] = name[j]; }
        assert(cursor + length + 1 <= 4096);
        text[cursor + length] = L',';
        cursor += (length + 1);


        font_family->Release();
        family_names->Release();

        OutputDebugStringW(name);
        OutputDebugStringW(L"\n");
        delete [] name;
    }
    UINT32 text_length = cursor;


    ID2D1HwndRenderTarget *rt;
    ID2D1SolidColorBrush *black_brush;

    RECT rect;
    GetClientRect(hwnd, &rect);
    UINT32 width = rect.right - rect.left;
    UINT32 height = rect.bottom - rect.top;

    D2D1_SIZE_U size = D2D1::SizeU(width, height);

    assert_hr(hr = d2d_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                  D2D1::HwndRenderTargetProperties(hwnd, size),
                                                  &rt));


    rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &black_brush);

    FLOAT dpi_x, dpi_y;
    rt->GetDpi(&dpi_x, &dpi_y);

    D2D1_RECT_F layout_rect = D2D1::RectF((FLOAT)rect.left / dpi_x, (FLOAT)rect.top  / dpi_y,
                                          (FLOAT)rect.right / dpi_x, (FLOAT)rect.bottom / dpi_y);


    assert_hr(hr = text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
    assert_hr(hr = text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));

    // @NOTE: Draw
    rt->BeginDraw();
    rt->SetTransform(D2D1::IdentityMatrix());
    rt->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    if (use_dwrite_text_layout)
    {
        IDWriteTextLayout *text_layout;
        assert_hr(hr = dwrite_factory->CreateTextLayout(text, text_length, text_format, width, height, &text_layout));

        DWRITE_TEXT_RANGE text_range = {10, 15};
        assert_hr(hr = text_layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, text_range));

        D2D1_POINT_2F origin = {rect.left / dpi_x, rect.top / dpi_y};

        rt->DrawTextLayout(origin, text_layout, black_brush); 
    }
    else
    { rt->DrawText(text, text_length, text_format, layout_rect, black_brush, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL); }
    assert_hr(hr = rt->EndDraw());


    // @NOTE: Main Loop
    for (;;)
    {
        bool done = false;
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
            { done = true; }
        }
        if (done)
        { break; }
    }

    return 0;
}
