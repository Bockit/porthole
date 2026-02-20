/*
 * Minimal test for DComp COM objects — works over SSH (no display needed).
 * Tests: device creation, visual creation, visual methods, QI, refcounting.
 * Does NOT test: target creation (needs HWND), swap chain, compositing.
 *
 * Compile: x86_64-w64-mingw32-gcc -o test_dcomp_minimal.exe test_dcomp_minimal.c \
 *          -lole32 -luuid
 * Run:     ./run_wine.sh tests/test_dcomp_minimal.exe
 */

#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>

/* DComp GUIDs */
DEFINE_GUID(IID_IDCompositionDevice,        0xc37ea93a,0xe7aa,0x450d,0xb1,0x6f,0x97,0x46,0xcb,0x04,0x07,0xf3);
DEFINE_GUID(IID_IDCompositionDevice2,       0x75f6468d,0x1b8e,0x447c,0x9b,0xc6,0x75,0xfe,0xa8,0x0b,0x5b,0x25);
DEFINE_GUID(IID_IDCompositionDesktopDevice, 0x5f4633fe,0x1e08,0x4cb8,0x8c,0x75,0xce,0x24,0x33,0x3f,0x56,0x02);
DEFINE_GUID(IID_IDCompositionDevice3,       0x0987cb06,0xf916,0x48bf,0x8d,0x35,0xce,0x76,0x41,0x78,0x1b,0xd9);
DEFINE_GUID(IID_IDCompositionVisual,        0x4d93059d,0x097b,0x4651,0x9a,0x60,0xf0,0xf2,0x51,0x16,0xe2,0xf3);
DEFINE_GUID(IID_IDCompositionVisual2,       0xe8de1639,0x4331,0x4b26,0xbc,0x5f,0x6a,0x32,0x1d,0x34,0x7a,0x85);

/* Minimal COM interface definitions */
typedef struct IDCompositionVisual2Vtbl IDCompositionVisual2Vtbl;
typedef struct IDCompositionDesktopDeviceVtbl IDCompositionDesktopDeviceVtbl;

typedef struct IDCompositionVisual2 {
    const IDCompositionVisual2Vtbl *lpVtbl;
} IDCompositionVisual2;

typedef struct IDCompositionDesktopDevice {
    const IDCompositionDesktopDeviceVtbl *lpVtbl;
} IDCompositionDesktopDevice;

/* IDCompositionVisual2 vtable — matches Wine IDL order */
struct IDCompositionVisual2Vtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionVisual2 *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDCompositionVisual2 *);
    ULONG   (STDMETHODCALLTYPE *Release)(IDCompositionVisual2 *);
    /* IDCompositionVisual — order must match IDL */
    HRESULT (STDMETHODCALLTYPE *SetOffsetXAnimation)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetX)(IDCompositionVisual2 *, float);
    HRESULT (STDMETHODCALLTYPE *SetOffsetYAnimation)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetY)(IDCompositionVisual2 *, float);
    HRESULT (STDMETHODCALLTYPE *SetTransformObject)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetTransform)(IDCompositionVisual2 *, const void *);
    HRESULT (STDMETHODCALLTYPE *SetTransformParent)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetEffect)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetBitmapInterpolationMode)(IDCompositionVisual2 *, int);
    HRESULT (STDMETHODCALLTYPE *SetBorderMode)(IDCompositionVisual2 *, int);
    HRESULT (STDMETHODCALLTYPE *SetClipObject)(IDCompositionVisual2 *, void *);
    HRESULT (STDMETHODCALLTYPE *SetClip)(IDCompositionVisual2 *, const void *);
    HRESULT (STDMETHODCALLTYPE *SetContent)(IDCompositionVisual2 *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *AddVisual)(IDCompositionVisual2 *, IDCompositionVisual2 *, BOOL, IDCompositionVisual2 *);
    HRESULT (STDMETHODCALLTYPE *RemoveVisual)(IDCompositionVisual2 *, IDCompositionVisual2 *);
    HRESULT (STDMETHODCALLTYPE *RemoveAllVisuals)(IDCompositionVisual2 *);
    HRESULT (STDMETHODCALLTYPE *SetCompositeMode)(IDCompositionVisual2 *, int);
    /* IDCompositionVisual2 */
    HRESULT (STDMETHODCALLTYPE *SetOpacityMode)(IDCompositionVisual2 *, int);
    HRESULT (STDMETHODCALLTYPE *SetBackFaceVisibility)(IDCompositionVisual2 *, int);
};

/* IDCompositionDesktopDevice vtable — matches Wine IDL order */
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
    HRESULT (STDMETHODCALLTYPE *CreateSurface)(IDCompositionDesktopDevice *, UINT, UINT, int, int, void **);
    HRESULT (STDMETHODCALLTYPE *CreateVirtualSurface)(IDCompositionDesktopDevice *, UINT, UINT, int, int, void **);
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
    HRESULT (STDMETHODCALLTYPE *CreateTargetForHwnd)(IDCompositionDesktopDevice *, HWND, BOOL, void **);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceFromHandle)(IDCompositionDesktopDevice *, HANDLE, void **);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceFromHwnd)(IDCompositionDesktopDevice *, HWND, void **);
};

typedef HRESULT (WINAPI *PFN_DCompositionCreateDevice3)(IUnknown *, REFIID, void **);

/* Test infrastructure */
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

int main(void)
{
    HRESULT hr;
    IDCompositionDesktopDevice *device = NULL;
    IUnknown *device3 = NULL;
    IUnknown *device1 = NULL;
    IDCompositionVisual2 *visual1 = NULL;
    IDCompositionVisual2 *visual2 = NULL;
    IDCompositionVisual2 *visual3 = NULL;
    HMODULE dcomp_dll;
    PFN_DCompositionCreateDevice3 pDCompositionCreateDevice3;

    printf("=== DComp Minimal Test (no display required) ===\n\n");

    /* --- Stage 1: Load dcomp.dll --- */
    printf("--- Stage 1: Load dcomp.dll ---\n");

    dcomp_dll = LoadLibraryW(L"dcomp.dll");
    CHECK_BOOL("LoadLibrary dcomp.dll", dcomp_dll != NULL);
    if (!dcomp_dll) goto done;

    pDCompositionCreateDevice3 = (PFN_DCompositionCreateDevice3)
        GetProcAddress(dcomp_dll, "DCompositionCreateDevice3");
    CHECK_BOOL("GetProcAddress DCompositionCreateDevice3", pDCompositionCreateDevice3 != NULL);
    if (!pDCompositionCreateDevice3) goto done;

    /* --- Stage 2: Device creation + QI --- */
    printf("\n--- Stage 2: Device Creation + QI ---\n");

    hr = pDCompositionCreateDevice3(NULL, &IID_IDCompositionDesktopDevice, (void **)&device);
    CHECK_HR("DCompositionCreateDevice3 -> IDCompositionDesktopDevice", hr);
    if (FAILED(hr)) goto done;

    /* QI for IDCompositionDevice3 */
    hr = device->lpVtbl->QueryInterface(device, &IID_IDCompositionDevice3, (void **)&device3);
    CHECK_HR("QI -> IDCompositionDevice3", hr);
    if (SUCCEEDED(hr))
        device3->lpVtbl->Release(device3);

    /* QI for IDCompositionDevice (v1) */
    hr = device->lpVtbl->QueryInterface(device, &IID_IDCompositionDevice, (void **)&device1);
    CHECK_HR("QI -> IDCompositionDevice (v1)", hr);
    if (SUCCEEDED(hr))
        device1->lpVtbl->Release(device1);

    /* QI for IDCompositionDevice2 */
    IUnknown *device2 = NULL;
    hr = device->lpVtbl->QueryInterface(device, &IID_IDCompositionDevice2, (void **)&device2);
    CHECK_HR("QI -> IDCompositionDevice2", hr);
    if (SUCCEEDED(hr))
        device2->lpVtbl->Release(device2);

    /* --- Stage 3: Visual creation + methods --- */
    printf("\n--- Stage 3: Visual Creation + Methods ---\n");

    hr = device->lpVtbl->CreateVisual(device, &visual1);
    CHECK_HR("CreateVisual (visual1)", hr);
    if (FAILED(hr)) goto done;

    hr = device->lpVtbl->CreateVisual(device, &visual2);
    CHECK_HR("CreateVisual (visual2)", hr);

    hr = device->lpVtbl->CreateVisual(device, &visual3);
    CHECK_HR("CreateVisual (visual3)", hr);

    /* Test visual methods — CEF calls all of these */
    hr = visual1->lpVtbl->SetOffsetX(visual1, 10.0f);
    CHECK_HR("Visual::SetOffsetX", hr);

    hr = visual1->lpVtbl->SetOffsetY(visual1, 20.0f);
    CHECK_HR("Visual::SetOffsetY", hr);

    hr = visual1->lpVtbl->SetBitmapInterpolationMode(visual1, 1);
    CHECK_HR("Visual::SetBitmapInterpolationMode", hr);

    hr = visual1->lpVtbl->SetBorderMode(visual1, 1);
    CHECK_HR("Visual::SetBorderMode", hr);

    hr = visual1->lpVtbl->SetOpacityMode(visual1, 0);
    CHECK_HR("Visual::SetOpacityMode", hr);

    hr = visual1->lpVtbl->SetBackFaceVisibility(visual1, 0);
    CHECK_HR("Visual::SetBackFaceVisibility", hr);

    hr = visual1->lpVtbl->SetCompositeMode(visual1, 0);
    CHECK_HR("Visual::SetCompositeMode", hr);

    /* --- Stage 4: Visual tree operations --- */
    printf("\n--- Stage 4: Visual Tree ---\n");

    hr = visual1->lpVtbl->AddVisual(visual1, visual2, TRUE, NULL);
    CHECK_HR("AddVisual (visual2 to visual1)", hr);

    hr = visual1->lpVtbl->AddVisual(visual1, visual3, TRUE, NULL);
    CHECK_HR("AddVisual (visual3 to visual1)", hr);

    hr = visual1->lpVtbl->RemoveVisual(visual1, visual2);
    CHECK_HR("RemoveVisual (visual2 from visual1)", hr);

    hr = visual1->lpVtbl->RemoveAllVisuals(visual1);
    CHECK_HR("RemoveAllVisuals", hr);

    /* --- Stage 5: SetContent with NULL (valid) --- */
    printf("\n--- Stage 5: SetContent ---\n");

    hr = visual1->lpVtbl->SetContent(visual1, NULL);
    CHECK_HR("Visual::SetContent(NULL)", hr);

    /* --- Stage 6: Commit (no-op in Phase 1) --- */
    printf("\n--- Stage 6: Commit ---\n");

    hr = device->lpVtbl->Commit(device);
    CHECK_HR("Device::Commit", hr);

    hr = device->lpVtbl->WaitForCommitCompletion(device);
    CHECK_HR("Device::WaitForCommitCompletion", hr);

    /* --- Stage 7: Visual QI --- */
    printf("\n--- Stage 7: Visual QI ---\n");

    IUnknown *vis_unk = NULL;
    hr = visual1->lpVtbl->QueryInterface(visual1, &IID_IUnknown, (void **)&vis_unk);
    CHECK_HR("Visual QI -> IUnknown", hr);
    if (SUCCEEDED(hr))
        vis_unk->lpVtbl->Release(vis_unk);

    IUnknown *vis_v1 = NULL;
    hr = visual1->lpVtbl->QueryInterface(visual1, &IID_IDCompositionVisual, (void **)&vis_v1);
    CHECK_HR("Visual QI -> IDCompositionVisual", hr);
    if (SUCCEEDED(hr))
        vis_v1->lpVtbl->Release(vis_v1);

    IDCompositionVisual2 *vis_v2 = NULL;
    hr = visual1->lpVtbl->QueryInterface(visual1, &IID_IDCompositionVisual2, (void **)&vis_v2);
    CHECK_HR("Visual QI -> IDCompositionVisual2", hr);
    if (SUCCEEDED(hr))
        vis_v2->lpVtbl->Release(vis_v2);

done:
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    if (visual3) visual3->lpVtbl->Release(visual3);
    if (visual2) visual2->lpVtbl->Release(visual2);
    if (visual1) visual1->lpVtbl->Release(visual1);
    if (device) device->lpVtbl->Release(device);

    return tests_failed > 0 ? 1 : 0;
}
