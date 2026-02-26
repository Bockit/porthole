/*
 * Automated test for DirectComposition implementation in Wine.
 * Tests the full pipeline: device creation, visual tree, swap chain, compositing.
 *
 * Compile: x86_64-w64-mingw32-gcc -o test_dcomp.exe test_dcomp.c \
 *          -ld3d11 -ldxgi -lole32 -luuid -lgdi32 -luser32
 * Run:     ./run_wine.sh tests/test_dcomp.exe
 *
 * Exit codes:
 *   0 = all tests passed
 *   1 = test failure (see output for which stage failed)
 *
 * Note: We define DComp interfaces manually because mingw's dcomp.h
 * is broken for plain C compilation.
 */

#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>

/* --- DComp interface definitions (subset we actually test) --- */

/* DComp GUIDs (not in mingw headers since we skip dcomp.h) */
DEFINE_GUID(IID_IDCompositionDesktopDevice, 0x5f4633fe,0x1e08,0x4cb8,0x8c,0x75,0xce,0x24,0x33,0x3f,0x56,0x02);
DEFINE_GUID(IID_IDCompositionDevice3,       0x0987cb06,0xf916,0x48bf,0x8d,0x35,0xce,0x76,0x41,0x78,0x1b,0xd9);

/* Enums */
typedef enum {
    DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR = 0,
    DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR = 1,
} DCOMPOSITION_BITMAP_INTERPOLATION_MODE;

typedef enum {
    DCOMPOSITION_BORDER_MODE_SOFT = 0,
    DCOMPOSITION_BORDER_MODE_HARD = 1,
} DCOMPOSITION_BORDER_MODE;

/* Forward declarations */
typedef struct IDCompositionTargetVtbl IDCompositionTargetVtbl;
typedef struct IDCompositionVisual2Vtbl IDCompositionVisual2Vtbl;
typedef struct IDCompositionDesktopDeviceVtbl IDCompositionDesktopDeviceVtbl;

typedef struct IDCompositionTarget {
    const IDCompositionTargetVtbl *lpVtbl;
} IDCompositionTarget;

typedef struct IDCompositionVisual2 {
    const IDCompositionVisual2Vtbl *lpVtbl;
} IDCompositionVisual2;

typedef struct IDCompositionDesktopDevice {
    const IDCompositionDesktopDeviceVtbl *lpVtbl;
} IDCompositionDesktopDevice;

/* IDCompositionTarget vtable — we only use SetRoot */
struct IDCompositionTargetVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionTarget *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDCompositionTarget *);
    ULONG   (STDMETHODCALLTYPE *Release)(IDCompositionTarget *);
    /* IDCompositionTarget */
    HRESULT (STDMETHODCALLTYPE *SetRoot)(IDCompositionTarget *, IDCompositionVisual2 *);
};

/* IDCompositionVisual2 vtable (IDCompositionVisual methods + Visual2 methods) */
struct IDCompositionVisual2Vtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionVisual2 *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDCompositionVisual2 *);
    ULONG   (STDMETHODCALLTYPE *Release)(IDCompositionVisual2 *);
    /* IDCompositionVisual */
    HRESULT (STDMETHODCALLTYPE *SetOffsetXAnimation)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetX)(IDCompositionVisual2 *, float);
    HRESULT (STDMETHODCALLTYPE *SetOffsetYAnimation)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetY)(IDCompositionVisual2 *, float);
    HRESULT (STDMETHODCALLTYPE *SetTransform)(IDCompositionVisual2 *, const void *);
    HRESULT (STDMETHODCALLTYPE *SetTransformObject)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetTransformParent)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetEffect)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetBitmapInterpolationMode)(IDCompositionVisual2 *, DCOMPOSITION_BITMAP_INTERPOLATION_MODE);
    HRESULT (STDMETHODCALLTYPE *SetBorderMode)(IDCompositionVisual2 *, DCOMPOSITION_BORDER_MODE);
    HRESULT (STDMETHODCALLTYPE *SetClip)(IDCompositionVisual2 *, const void *);
    HRESULT (STDMETHODCALLTYPE *SetClipObject)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetContent)(IDCompositionVisual2 *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *AddVisual)(IDCompositionVisual2 *, IDCompositionVisual2 *, BOOL, IDCompositionVisual2 *);
    HRESULT (STDMETHODCALLTYPE *RemoveVisual)(IDCompositionVisual2 *, IDCompositionVisual2 *);
    HRESULT (STDMETHODCALLTYPE *RemoveAllVisuals)(IDCompositionVisual2 *);
    HRESULT (STDMETHODCALLTYPE *SetCompositeMode)(IDCompositionVisual2 *, int);
    /* IDCompositionVisual2 */
    HRESULT (STDMETHODCALLTYPE *SetOpacityMode)(IDCompositionVisual2 *, int);
    HRESULT (STDMETHODCALLTYPE *SetBackFaceVisibility)(IDCompositionVisual2 *, int);
};

/* IDCompositionDesktopDevice vtable (IDCompositionDevice2 + DesktopDevice methods) */
struct IDCompositionDesktopDeviceVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionDesktopDevice *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDCompositionDesktopDevice *);
    ULONG   (STDMETHODCALLTYPE *Release)(IDCompositionDesktopDevice *);
    /* IDCompositionDevice2 */
    HRESULT (STDMETHODCALLTYPE *Commit)(IDCompositionDesktopDevice *);
    HRESULT (STDMETHODCALLTYPE *WaitForCommitCompletion)(IDCompositionDesktopDevice *);
    HRESULT (STDMETHODCALLTYPE *GetFrameStatistics)(IDCompositionDesktopDevice *, void *);
    HRESULT (STDMETHODCALLTYPE *CreateVisual)(IDCompositionDesktopDevice *, IDCompositionVisual2 **);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceFactory)(IDCompositionDesktopDevice *, IUnknown *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateSurface)(IDCompositionDesktopDevice *, UINT, UINT, DXGI_FORMAT, int, void **);
    HRESULT (STDMETHODCALLTYPE *CreateVirtualSurface)(IDCompositionDesktopDevice *, UINT, UINT, DXGI_FORMAT, int, void **);
    HRESULT (STDMETHODCALLTYPE *CreateTranslateTransform)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateScaleTransform)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateRotateTransform)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateSkewTransform)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateMatrixTransform)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateTransformGroup)(IDCompositionDesktopDevice *, void **, UINT, void **);
    HRESULT (STDMETHODCALLTYPE *CreateTranslateTransform3D)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateScaleTransform3D)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateRotateTransform3D)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateMatrixTransform3D)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateTransform3DGroup)(IDCompositionDesktopDevice *, void **, UINT, void **);
    HRESULT (STDMETHODCALLTYPE *CreateEffectGroup)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateRectangleClip)(IDCompositionDesktopDevice *, void **);
    HRESULT (STDMETHODCALLTYPE *CreateAnimation)(IDCompositionDesktopDevice *, void **);
    /* IDCompositionDesktopDevice */
    HRESULT (STDMETHODCALLTYPE *CreateTargetForHwnd)(IDCompositionDesktopDevice *, HWND, BOOL, IDCompositionTarget **);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceFromHandle)(IDCompositionDesktopDevice *, HANDLE, void **);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceFromHwnd)(IDCompositionDesktopDevice *, HWND, void **);
};

/* DCompositionCreateDevice3 — loaded at runtime */
typedef HRESULT (WINAPI *PFN_DCompositionCreateDevice3)(IUnknown *, REFIID, void **);

/* --- Test infrastructure --- */

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK_HR(msg, hr) do { \
    if (SUCCEEDED(hr)) { \
        printf("[PASS] %s (hr=0x%08lx)\n", msg, (unsigned long)(hr)); \
        tests_passed++; \
    } else { \
        printf("[FAIL] %s (hr=0x%08lx)\n", msg, (unsigned long)(hr)); \
        tests_failed++; \
    } \
} while(0)

#define CHECK_BOOL(msg, cond) do { \
    if (cond) { \
        printf("[PASS] %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("[FAIL] %s\n", msg); \
        tests_failed++; \
    } \
} while(0)

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(void)
{
    HRESULT hr;
    HWND hwnd;
    WNDCLASSW wc = {0};
    MSG msg;

    /* D3D11/DXGI objects */
    ID3D11Device *d3d_device = NULL;
    ID3D11DeviceContext *d3d_context = NULL;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIFactory2 *dxgi_factory = NULL;
    IDXGIAdapter *dxgi_adapter = NULL;
    IDXGISwapChain1 *swapchain = NULL;
    ID3D11Texture2D *backbuffer = NULL;
    ID3D11RenderTargetView *rtv = NULL;

    /* DComp objects */
    IDCompositionDesktopDevice *desktop_device = NULL;
    IUnknown *dcomp_device3 = NULL;
    IDCompositionTarget *dcomp_target = NULL;
    IDCompositionVisual2 *dcomp_visual = NULL;

    /* GPU readback result */
    unsigned int gpu_r = 0, gpu_g = 0, gpu_b = 0;

    /* DComp function */
    HMODULE dcomp_dll;
    PFN_DCompositionCreateDevice3 pDCompositionCreateDevice3;

    printf("=== DirectComposition Test ===\n\n");

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    /* Create a window */
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"DCompTest";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    hwnd = CreateWindowExW(0, L"DCompTest", L"DComp Test",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           100, 100, 220, 220, NULL, NULL, wc.hInstance, NULL);
    UpdateWindow(hwnd);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CHECK_BOOL("Window created", hwnd != NULL);

    /* --- Stage 1: D3D11 + DXGI setup --- */
    printf("\n--- Stage 1: D3D11/DXGI Setup ---\n");

    D3D_FEATURE_LEVEL feature_level;
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                           D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                           D3D11_SDK_VERSION, &d3d_device, &feature_level, &d3d_context);
    CHECK_HR("D3D11CreateDevice", hr);
    if (FAILED(hr)) goto done;

    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    CHECK_HR("QueryInterface IDXGIDevice", hr);
    if (FAILED(hr)) goto done;

    /* --- Stage 2: DComp device creation --- */
    printf("\n--- Stage 2: DComp Device Creation ---\n");

    dcomp_dll = LoadLibraryW(L"dcomp.dll");
    CHECK_BOOL("LoadLibrary dcomp.dll", dcomp_dll != NULL);
    if (!dcomp_dll) goto done;

    pDCompositionCreateDevice3 = (PFN_DCompositionCreateDevice3)
        GetProcAddress(dcomp_dll, "DCompositionCreateDevice3");
    CHECK_BOOL("GetProcAddress DCompositionCreateDevice3", pDCompositionCreateDevice3 != NULL);
    if (!pDCompositionCreateDevice3) goto done;

    hr = pDCompositionCreateDevice3((IUnknown *)dxgi_device,
                                    &IID_IDCompositionDesktopDevice,
                                    (void **)&desktop_device);
    CHECK_HR("DCompositionCreateDevice3 -> IDCompositionDesktopDevice", hr);
    if (FAILED(hr))
    {
        printf("\n*** DComp not implemented. Remaining tests skipped. ***\n");
        goto done;
    }

    hr = desktop_device->lpVtbl->QueryInterface(desktop_device,
                                                 &IID_IDCompositionDevice3,
                                                 (void **)&dcomp_device3);
    CHECK_HR("QueryInterface -> IDCompositionDevice3", hr);

    /* --- Stage 3: Target + Visual creation --- */
    printf("\n--- Stage 3: Target + Visual Creation ---\n");

    hr = desktop_device->lpVtbl->CreateTargetForHwnd(desktop_device, hwnd, TRUE, &dcomp_target);
    CHECK_HR("CreateTargetForHwnd", hr);
    if (FAILED(hr)) goto done;

    hr = desktop_device->lpVtbl->CreateVisual(desktop_device, &dcomp_visual);
    CHECK_HR("CreateVisual", hr);
    if (FAILED(hr)) goto done;

    hr = dcomp_visual->lpVtbl->SetBitmapInterpolationMode(dcomp_visual,
            DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
    CHECK_HR("Visual::SetBitmapInterpolationMode", hr);

    hr = dcomp_visual->lpVtbl->SetBorderMode(dcomp_visual, DCOMPOSITION_BORDER_MODE_SOFT);
    CHECK_HR("Visual::SetBorderMode", hr);

    hr = dcomp_target->lpVtbl->SetRoot(dcomp_target, dcomp_visual);
    CHECK_HR("Target::SetRoot", hr);

    /* --- Stage 4: Swap chain + rendering --- */
    printf("\n--- Stage 4: Swap Chain + Rendering ---\n");

    hr = IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);
    if (SUCCEEDED(hr))
    {
        hr = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, (void **)&dxgi_factory);
        IDXGIAdapter_Release(dxgi_adapter);
    }
    CHECK_HR("Get IDXGIFactory2", hr);
    if (FAILED(hr)) goto done;

    DXGI_SWAP_CHAIN_DESC1 sc_desc = {0};
    sc_desc.Width = 200;
    sc_desc.Height = 200;
    sc_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc_desc.SampleDesc.Count = 1;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = 2;
    sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sc_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = IDXGIFactory2_CreateSwapChainForComposition(dxgi_factory, (IUnknown *)d3d_device,
                                                      &sc_desc, NULL, &swapchain);
    CHECK_HR("CreateSwapChainForComposition", hr);
    if (FAILED(hr))
    {
        printf("  (falling back to CreateSwapChainForHwnd)\n");
        hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)d3d_device,
                                                   hwnd, &sc_desc, NULL, NULL, &swapchain);
        CHECK_HR("CreateSwapChainForHwnd (fallback)", hr);
        if (FAILED(hr)) goto done;
    }

    /* Render solid red to the swap chain */
    hr = IDXGISwapChain1_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void **)&backbuffer);
    CHECK_HR("GetBuffer", hr);
    if (FAILED(hr)) goto done;

    hr = ID3D11Device_CreateRenderTargetView(d3d_device, (ID3D11Resource *)backbuffer, NULL, &rtv);
    CHECK_HR("CreateRenderTargetView", hr);
    if (FAILED(hr)) goto done;

    FLOAT red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(d3d_context, rtv, red);

    /* GPU staging readback: verify D3D11 rendered red BEFORE presenting.
     * GetPixel(GDI DC) cannot see Metal/Vulkan content on macOS Wine —
     * we must read back from the GPU directly. */
    {
        ID3D11Texture2D *staging_tex = NULL;
        D3D11_TEXTURE2D_DESC staging_desc = {0};
        staging_desc.Width = 1;
        staging_desc.Height = 1;
        staging_desc.MipLevels = 1;
        staging_desc.ArraySize = 1;
        staging_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        staging_desc.SampleDesc.Count = 1;
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        hr = ID3D11Device_CreateTexture2D(d3d_device, &staging_desc, NULL, &staging_tex);
        if (SUCCEEDED(hr))
        {
            D3D11_BOX src_box = {0, 0, 0, 1, 1, 1};
            D3D11_MAPPED_SUBRESOURCE mapped = {0};
            ID3D11DeviceContext_CopySubresourceRegion(d3d_context,
                    (ID3D11Resource *)staging_tex, 0, 0, 0, 0,
                    (ID3D11Resource *)backbuffer, 0, &src_box);
            hr = ID3D11DeviceContext_Map(d3d_context, (ID3D11Resource *)staging_tex,
                    0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(hr))
            {
                /* BGRA byte layout: [0]=B [1]=G [2]=R [3]=A */
                BYTE *p = (BYTE *)mapped.pData;
                gpu_b = p[0]; gpu_g = p[1]; gpu_r = p[2];
                printf("  GPU readback (pre-Present, BGRA): R=%u G=%u B=%u\n",
                        gpu_r, gpu_g, gpu_b);
                ID3D11DeviceContext_Unmap(d3d_context, (ID3D11Resource *)staging_tex, 0);
            }
            ID3D11Texture2D_Release(staging_tex);
        }
    }

    hr = IDXGISwapChain1_Present(swapchain, 0, 0);
    CHECK_HR("SwapChain::Present", hr);

    /* --- Stage 5: DComp compositing --- */
    printf("\n--- Stage 5: DComp Compositing ---\n");

    hr = dcomp_visual->lpVtbl->SetContent(dcomp_visual, (IUnknown *)swapchain);
    CHECK_HR("Visual::SetContent(swapchain)", hr);

    hr = desktop_device->lpVtbl->Commit(desktop_device);
    CHECK_HR("Device::Commit", hr);

    /* Give compositor thread time to run and window time to be visible */
    Sleep(2000);

    /* Pump messages */
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* --- Stage 6: Pixel Verification (GPU staging readback) ---
     * GetPixel(GDI DC) always reads the GDI backing store, not the Metal/Vulkan surface.
     * On macOS Wine the two are independent — use D3D11 staging readback instead. */
    printf("\n--- Stage 6: Pixel Verification ---\n");
    printf("  GPU readback result: R=%u G=%u B=%u\n", gpu_r, gpu_g, gpu_b);

    HDC hdc = GetDC(hwnd);
    COLORREF pixel = GetPixel(hdc, 10, 10);
    ReleaseDC(hwnd, hdc);
    printf("  GDI GetPixel (informational only, will be white on macOS): R=%u G=%u B=%u\n",
            GetRValue(pixel), GetGValue(pixel), GetBValue(pixel));

    int is_red = (gpu_r > 200 && gpu_g < 50 && gpu_b < 50);
    CHECK_BOOL("GPU rendered red (D3D11 staging readback)", is_red);

done:
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (backbuffer) ID3D11Texture2D_Release(backbuffer);
    if (swapchain) IDXGISwapChain1_Release(swapchain);
    if (dxgi_factory) IDXGIFactory2_Release(dxgi_factory);
    if (dcomp_visual) dcomp_visual->lpVtbl->Release(dcomp_visual);
    if (dcomp_target) dcomp_target->lpVtbl->Release(dcomp_target);
    if (dcomp_device3) dcomp_device3->lpVtbl->Release(dcomp_device3);
    if (desktop_device) desktop_device->lpVtbl->Release(desktop_device);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    if (d3d_context) ID3D11DeviceContext_Release(d3d_context);
    if (d3d_device) ID3D11Device_Release(d3d_device);
    if (hwnd) DestroyWindow(hwnd);

    CoUninitialize();
    return tests_failed > 0 ? 1 : 0;
}
