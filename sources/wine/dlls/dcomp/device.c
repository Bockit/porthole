/*
 * Copyright 2020 Nikolay Sivov for CodeWeavers
 * Copyright 2023 Zhiyi Zhang for CodeWeavers
 * Copyright 2026 Porthole contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#include <stdarg.h>

#include "initguid.h"

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "dxgi.h"
#include "dcomp_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dcomp);

/* Thread-local storage: HWND of the most recently created composition target on this thread.
 * Set by CreateTargetForHwnd, read by __wine_dcomp_get_target_hwnd() which factory.c calls
 * so that CreateSwapChainForComposition can target the right window from the start. */
static LONG tls_target_hwnd_index = -1; /* -1 == TLS_OUT_OF_INDEXES */

static DWORD get_tls_index(void)
{
    if (tls_target_hwnd_index == -1)
    {
        DWORD idx = TlsAlloc();
        if (InterlockedCompareExchange(&tls_target_hwnd_index, (LONG)idx, -1) != -1)
            TlsFree(idx); /* another thread beat us to it */
    }
    return (DWORD)tls_target_hwnd_index;
}

void dcomp_set_current_target_hwnd(HWND hwnd)
{
    DWORD idx = get_tls_index();
    if (idx != TLS_OUT_OF_INDEXES)
        TlsSetValue(idx, (LPVOID)hwnd);
}

HWND CDECL __wine_dcomp_get_target_hwnd(void)
{
    DWORD idx = get_tls_index();
    if (idx == TLS_OUT_OF_INDEXES)
        return NULL;
    return (HWND)TlsGetValue(idx);
}

/*
 * IDCompositionDevice (v1) vtable implementation
 *
 * Method order from dcomp.idl IDCompositionDevice:
 *   IUnknown: QueryInterface, AddRef, Release
 *   Commit, WaitForCommitCompletion, GetFrameStatistics,
 *   CreateTargetForHwnd, CreateVisual, CreateSurface, CreateVirtualSurface,
 *   CreateSurfaceFromHandle, CreateSurfaceFromHwnd,
 *   CreateTranslateTransform, CreateScaleTransform, CreateRotateTransform,
 *   CreateSkewTransform, CreateMatrixTransform, CreateTransformGroup,
 *   CreateTranslateTransform3D, CreateScaleTransform3D, CreateRotateTransform3D,
 *   CreateMatrixTransform3D, CreateTransform3DGroup,
 *   CreateEffectGroup, CreateRectangleClip, CreateAnimation,
 *   CheckDeviceState
 */

static HRESULT STDMETHODCALLTYPE device1_QueryInterface(IDCompositionDevice *iface, REFIID iid,
        void **out)
{
    struct composition_device *device = impl_from_IDCompositionDevice(iface);

    TRACE("iface %p, iid %s, out %p\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IDCompositionDevice))
    {
        IUnknown_AddRef(&device->IDCompositionDevice_iface);
        *out = &device->IDCompositionDevice_iface;
        return S_OK;
    }

    if (device->version >= 2
            && (IsEqualGUID(iid, &IID_IDCompositionDevice2)
                || IsEqualGUID(iid, &IID_IDCompositionDesktopDevice)))
    {
        IUnknown_AddRef(&device->IDCompositionDesktopDevice_iface);
        *out = &device->IDCompositionDesktopDevice_iface;
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_IDCompositionDevice3))
    {
        FIXME("IDCompositionDevice3 not implemented, returning E_NOINTERFACE.\n");
        *out = NULL;
        return E_NOINTERFACE;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE device1_AddRef(IDCompositionDevice *iface)
{
    struct composition_device *device = impl_from_IDCompositionDevice(iface);
    ULONG ref = InterlockedIncrement(&device->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE device1_Release(IDCompositionDevice *iface)
{
    struct composition_device *device = impl_from_IDCompositionDevice(iface);
    ULONG ref = InterlockedDecrement(&device->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);

    if (!ref)
    {
        if (device->thread)
        {
            WaitForSingleObject(device->thread, INFINITE);
            CloseHandle(device->thread);
        }
        DeleteCriticalSection(&device->cs);
        free(device);
    }

    return ref;
}

/* Find the first visual in a tree that has swap chain content (recursive DFS). */
static struct composition_visual *find_content_visual(struct composition_visual *visual)
{
    struct visual_child *child;
    struct composition_visual *result;

    if (visual->content)
        return visual;

    LIST_FOR_EACH_ENTRY(child, &visual->children, struct visual_child, entry)
    {
        struct composition_visual *child_visual = impl_from_IDCompositionVisual2(child->visual);
        result = find_content_visual(child_visual);
        if (result)
            return result;
    }

    return NULL;
}

/* Maximum number of composition targets processed per Commit. */
#define MAX_COMPOSITE_TARGETS 32

/* Snapshot of a single target's compositing work, collected under the device lock. */
struct composite_snapshot
{
    HWND target_hwnd;
    IUnknown *content; /* AddRef'd; caller must Release after use */
    float offset_x;
    float offset_y;
};

/* Reparent the swap chain's window into the target HWND so its Vulkan/Metal
 * surface becomes visible. Called WITHOUT the device lock held. */
static void do_composite_work(const struct composite_snapshot *work)
{
    IDXGISwapChain *swapchain = NULL;
    DXGI_SWAP_CHAIN_DESC desc;
    HWND swap_hwnd;
    RECT rect;
    HRESULT hr;

    hr = IUnknown_QueryInterface(work->content, &IID_IDXGISwapChain, (void **)&swapchain);
    if (FAILED(hr))
    {
        FIXME("Visual content %p is not an IDXGISwapChain, hr %#lx\n", work->content, hr);
        return;
    }

    hr = IDXGISwapChain_GetDesc(swapchain, &desc);
    IDXGISwapChain_Release(swapchain);
    if (FAILED(hr))
    {
        ERR("Failed to get swap chain desc, hr %#lx\n", hr);
        return;
    }

    swap_hwnd = desc.OutputWindow;
    TRACE("swap chain window %p, size %ux%u, target hwnd %p\n",
            swap_hwnd, desc.BufferDesc.Width, desc.BufferDesc.Height, work->target_hwnd);

    if (!swap_hwnd || !IsWindow(swap_hwnd))
    {
        ERR("Swap chain has no valid output window %p\n", swap_hwnd);
        return;
    }

    /* If the swap chain was created directly for the target window (via __wine_dcomp_get_target_hwnd),
     * the Vulkan surface is already on the right NSView — no reparenting needed. */
    if (swap_hwnd == work->target_hwnd)
    {
        TRACE("swap chain window IS target hwnd %p, already rendering there\n", swap_hwnd);
        return;
    }

    GetClientRect(work->target_hwnd, &rect);
    TRACE("reparenting swap hwnd %p into target hwnd %p, client rect %ldx%ld\n",
            swap_hwnd, work->target_hwnd, rect.right - rect.left, rect.bottom - rect.top);
    SetParent(swap_hwnd, work->target_hwnd);
    SetWindowLongW(swap_hwnd, GWL_STYLE,
            (GetWindowLongW(swap_hwnd, GWL_STYLE) & ~WS_POPUP) | WS_CHILD | WS_VISIBLE);
    SetWindowPos(swap_hwnd, HWND_TOP,
            (int)work->offset_x, (int)work->offset_y,
            rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

static DWORD WINAPI composite_thread_proc(void *param)
{
    struct composition_device *device = impl_from_IDCompositionDevice((IDCompositionDevice *)param);
    struct composite_snapshot snapshots[MAX_COMPOSITE_TARGETS];
    struct composition_target *target;
    unsigned int n, i;

    TRACE("compositor thread started for device %p\n", device);

    /* Snapshot all targets that have content, under the device lock.
     * We AddRef each content object so it stays alive after we drop the lock. */
    n = 0;
    EnterCriticalSection(&device->cs);
    LIST_FOR_EACH_ENTRY(target, &device->targets, struct composition_target, entry)
    {
        struct composition_visual *root_visual, *content_visual;

        if (!target->root)
            continue;

        root_visual = impl_from_IDCompositionVisual(target->root);
        content_visual = find_content_visual(root_visual);
        if (!content_visual)
            continue;

        if (n < MAX_COMPOSITE_TARGETS)
        {
            snapshots[n].target_hwnd = target->hwnd;
            snapshots[n].content = content_visual->content;
            IUnknown_AddRef(snapshots[n].content);
            snapshots[n].offset_x = content_visual->offset_x;
            snapshots[n].offset_y = content_visual->offset_y;
            n++;
        }
    }
    LeaveCriticalSection(&device->cs);

    /* Perform all window operations outside the lock to avoid deadlock.
     * SetParent/SetWindowPos send messages to the target window's thread,
     * which may be blocked in Commit() trying to acquire device->cs. */
    for (i = 0; i < n; i++)
    {
        do_composite_work(&snapshots[i]);
        IUnknown_Release(snapshots[i].content);
    }

    if (!n)
        TRACE("compositor thread: no content found\n");
    else
        TRACE("compositor thread: composited %u target(s)\n", n);

    EnterCriticalSection(&device->cs);
    device->thread_exited = TRUE;
    LeaveCriticalSection(&device->cs);

    return 0;
}

static HRESULT STDMETHODCALLTYPE device1_Commit(IDCompositionDevice *iface)
{
    struct composition_device *device = impl_from_IDCompositionDevice(iface);

    TRACE("iface %p\n", iface);

    EnterCriticalSection(&device->cs);

    if (!device->thread || device->thread_exited)
    {
        if (device->thread)
        {
            WaitForSingleObject(device->thread, INFINITE);
            CloseHandle(device->thread);
        }
        device->thread_exited = FALSE;
        device->thread = CreateThread(NULL, 0, composite_thread_proc, iface, 0, NULL);
    }

    LeaveCriticalSection(&device->cs);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE device1_WaitForCommitCompletion(IDCompositionDevice *iface)
{
    TRACE("iface %p\n", iface);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE device1_GetFrameStatistics(IDCompositionDevice *iface,
        DCOMPOSITION_FRAME_STATISTICS *statistics)
{
    TRACE("iface %p, statistics %p\n", iface, statistics);

    if (!statistics)
        return E_INVALIDARG;

    memset(statistics, 0, sizeof(*statistics));
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE device1_CreateTargetForHwnd(IDCompositionDevice *iface,
        HWND hwnd, BOOL topmost, IDCompositionTarget **target)
{
    struct composition_device *device = impl_from_IDCompositionDevice(iface);

    TRACE("iface %p, hwnd %p, topmost %d, target %p\n", iface, hwnd, topmost, target);
    return create_target(device, hwnd, topmost, target);
}

static HRESULT STDMETHODCALLTYPE device1_CreateVisual(IDCompositionDevice *iface,
        IDCompositionVisual **visual)
{
    TRACE("iface %p, visual %p\n", iface, visual);
    return create_visual(1, &IID_IDCompositionVisual, (void **)visual);
}

static HRESULT STDMETHODCALLTYPE device1_CreateSurface(IDCompositionDevice *iface,
        UINT width, UINT height, DXGI_FORMAT pixel_format, DXGI_ALPHA_MODE alpha_mode,
        IDCompositionSurface **surface)
{
    FIXME("iface %p, %ux%u, format %u, alpha %u, surface %p: stub\n", iface, width, height,
            pixel_format, alpha_mode, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateVirtualSurface(IDCompositionDevice *iface,
        UINT width, UINT height, DXGI_FORMAT pixel_format, DXGI_ALPHA_MODE alpha_mode,
        IDCompositionVirtualSurface **surface)
{
    FIXME("iface %p, %ux%u, format %u, alpha %u, surface %p: stub\n", iface, width, height,
            pixel_format, alpha_mode, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateSurfaceFromHandle(IDCompositionDevice *iface,
        HANDLE handle, IUnknown **surface)
{
    FIXME("iface %p, handle %p, surface %p: stub\n", iface, handle, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateSurfaceFromHwnd(IDCompositionDevice *iface,
        HWND hwnd, IUnknown **surface)
{
    FIXME("iface %p, hwnd %p, surface %p: stub\n", iface, hwnd, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateTranslateTransform(IDCompositionDevice *iface,
        IDCompositionTranslateTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateScaleTransform(IDCompositionDevice *iface,
        IDCompositionScaleTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateRotateTransform(IDCompositionDevice *iface,
        IDCompositionRotateTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateSkewTransform(IDCompositionDevice *iface,
        IDCompositionSkewTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateMatrixTransform(IDCompositionDevice *iface,
        IDCompositionMatrixTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateTransformGroup(IDCompositionDevice *iface,
        IDCompositionTransform **transforms, UINT elements,
        IDCompositionTransform **transform_group)
{
    FIXME("iface %p, transforms %p, elements %u, transform_group %p: stub\n", iface,
            transforms, elements, transform_group);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateTranslateTransform3D(IDCompositionDevice *iface,
        IDCompositionTranslateTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateScaleTransform3D(IDCompositionDevice *iface,
        IDCompositionScaleTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateRotateTransform3D(IDCompositionDevice *iface,
        IDCompositionRotateTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateMatrixTransform3D(IDCompositionDevice *iface,
        IDCompositionMatrixTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateTransform3DGroup(IDCompositionDevice *iface,
        IDCompositionTransform3D **transforms_3d, UINT elements,
        IDCompositionTransform3D **transform_3d_group)
{
    FIXME("iface %p, transforms_3d %p, elements %u, transform_3d_group %p: stub\n", iface,
            transforms_3d, elements, transform_3d_group);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateEffectGroup(IDCompositionDevice *iface,
        IDCompositionEffectGroup **effect_group)
{
    FIXME("iface %p, effect_group %p: stub\n", iface, effect_group);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateRectangleClip(IDCompositionDevice *iface,
        IDCompositionRectangleClip **clip)
{
    FIXME("iface %p, clip %p: stub\n", iface, clip);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CreateAnimation(IDCompositionDevice *iface,
        IDCompositionAnimation **animation)
{
    FIXME("iface %p, animation %p: stub\n", iface, animation);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE device1_CheckDeviceState(IDCompositionDevice *iface,
        BOOL *valid)
{
    TRACE("iface %p, valid %p\n", iface, valid);

    if (!valid)
        return E_INVALIDARG;

    *valid = TRUE;
    return S_OK;
}

static const struct IDCompositionDeviceVtbl device1_vtbl =
{
    /* IUnknown methods */
    device1_QueryInterface,
    device1_AddRef,
    device1_Release,
    /* IDCompositionDevice methods */
    device1_Commit,
    device1_WaitForCommitCompletion,
    device1_GetFrameStatistics,
    device1_CreateTargetForHwnd,
    device1_CreateVisual,
    device1_CreateSurface,
    device1_CreateVirtualSurface,
    device1_CreateSurfaceFromHandle,
    device1_CreateSurfaceFromHwnd,
    device1_CreateTranslateTransform,
    device1_CreateScaleTransform,
    device1_CreateRotateTransform,
    device1_CreateSkewTransform,
    device1_CreateMatrixTransform,
    device1_CreateTransformGroup,
    device1_CreateTranslateTransform3D,
    device1_CreateScaleTransform3D,
    device1_CreateRotateTransform3D,
    device1_CreateMatrixTransform3D,
    device1_CreateTransform3DGroup,
    device1_CreateEffectGroup,
    device1_CreateRectangleClip,
    device1_CreateAnimation,
    device1_CheckDeviceState,
};

/*
 * IDCompositionDesktopDevice vtable implementation
 *
 * Inherits from IDCompositionDevice2 (which inherits IUnknown).
 * Method order from dcomp.idl:
 *   IUnknown: QueryInterface, AddRef, Release
 *   IDCompositionDevice2: Commit, WaitForCommitCompletion, GetFrameStatistics,
 *     CreateVisual, CreateSurfaceFactory, CreateSurface, CreateVirtualSurface,
 *     CreateTranslateTransform, CreateScaleTransform, CreateRotateTransform,
 *     CreateSkewTransform, CreateMatrixTransform, CreateTransformGroup,
 *     CreateTranslateTransform3D, CreateScaleTransform3D, CreateRotateTransform3D,
 *     CreateMatrixTransform3D, CreateTransform3DGroup,
 *     CreateEffectGroup, CreateRectangleClip, CreateAnimation
 *   IDCompositionDesktopDevice: CreateTargetForHwnd, CreateSurfaceFromHandle,
 *     CreateSurfaceFromHwnd
 */

static HRESULT STDMETHODCALLTYPE desktop_device_QueryInterface(IDCompositionDesktopDevice *iface,
        REFIID iid, void **out)
{
    struct composition_device *device = impl_from_IDCompositionDesktopDevice(iface);

    /* Delegate to the shared QI implementation on the v1 interface */
    return device1_QueryInterface(&device->IDCompositionDevice_iface, iid, out);
}

static ULONG STDMETHODCALLTYPE desktop_device_AddRef(IDCompositionDesktopDevice *iface)
{
    struct composition_device *device = impl_from_IDCompositionDesktopDevice(iface);
    ULONG ref = InterlockedIncrement(&device->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE desktop_device_Release(IDCompositionDesktopDevice *iface)
{
    struct composition_device *device = impl_from_IDCompositionDesktopDevice(iface);

    return device1_Release(&device->IDCompositionDevice_iface);
}

static HRESULT STDMETHODCALLTYPE desktop_device_Commit(IDCompositionDesktopDevice *iface)
{
    struct composition_device *device = impl_from_IDCompositionDesktopDevice(iface);

    return device1_Commit(&device->IDCompositionDevice_iface);
}

static HRESULT STDMETHODCALLTYPE desktop_device_WaitForCommitCompletion(
        IDCompositionDesktopDevice *iface)
{
    TRACE("iface %p\n", iface);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE desktop_device_GetFrameStatistics(
        IDCompositionDesktopDevice *iface, DCOMPOSITION_FRAME_STATISTICS *statistics)
{
    TRACE("iface %p, statistics %p\n", iface, statistics);

    if (!statistics)
        return E_INVALIDARG;

    memset(statistics, 0, sizeof(*statistics));
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateVisual(IDCompositionDesktopDevice *iface,
        IDCompositionVisual2 **visual)
{
    TRACE("iface %p, visual %p\n", iface, visual);
    return create_visual(2, &IID_IDCompositionVisual2, (void **)visual);
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateSurfaceFactory(
        IDCompositionDesktopDevice *iface, IUnknown *rendering_device,
        IDCompositionSurfaceFactory **surface_factory)
{
    FIXME("iface %p, rendering_device %p, surface_factory %p: stub\n", iface, rendering_device,
            surface_factory);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateSurface(IDCompositionDesktopDevice *iface,
        UINT width, UINT height, DXGI_FORMAT pixel_format, DXGI_ALPHA_MODE alpha_mode,
        IDCompositionSurface **surface)
{
    FIXME("iface %p, %ux%u, format %u, alpha %u, surface %p: stub\n", iface, width, height,
            pixel_format, alpha_mode, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateVirtualSurface(
        IDCompositionDesktopDevice *iface, UINT width, UINT height, DXGI_FORMAT pixel_format,
        DXGI_ALPHA_MODE alpha_mode, IDCompositionVirtualSurface **surface)
{
    FIXME("iface %p, %ux%u, format %u, alpha %u, surface %p: stub\n", iface, width, height,
            pixel_format, alpha_mode, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateTranslateTransform(
        IDCompositionDesktopDevice *iface, IDCompositionTranslateTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateScaleTransform(
        IDCompositionDesktopDevice *iface, IDCompositionScaleTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateRotateTransform(
        IDCompositionDesktopDevice *iface, IDCompositionRotateTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateSkewTransform(
        IDCompositionDesktopDevice *iface, IDCompositionSkewTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateMatrixTransform(
        IDCompositionDesktopDevice *iface, IDCompositionMatrixTransform **transform)
{
    FIXME("iface %p, transform %p: stub\n", iface, transform);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateTransformGroup(
        IDCompositionDesktopDevice *iface, IDCompositionTransform **transforms, UINT elements,
        IDCompositionTransform **transform_group)
{
    FIXME("iface %p, transforms %p, elements %u, transform_group %p: stub\n", iface,
            transforms, elements, transform_group);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateTranslateTransform3D(
        IDCompositionDesktopDevice *iface, IDCompositionTranslateTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateScaleTransform3D(
        IDCompositionDesktopDevice *iface, IDCompositionScaleTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateRotateTransform3D(
        IDCompositionDesktopDevice *iface, IDCompositionRotateTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateMatrixTransform3D(
        IDCompositionDesktopDevice *iface, IDCompositionMatrixTransform3D **transform_3d)
{
    FIXME("iface %p, transform_3d %p: stub\n", iface, transform_3d);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateTransform3DGroup(
        IDCompositionDesktopDevice *iface, IDCompositionTransform3D **transforms_3d, UINT elements,
        IDCompositionTransform3D **transform_3d_group)
{
    FIXME("iface %p, transforms_3d %p, elements %u, transform_3d_group %p: stub\n", iface,
            transforms_3d, elements, transform_3d_group);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateEffectGroup(
        IDCompositionDesktopDevice *iface, IDCompositionEffectGroup **effect_group)
{
    FIXME("iface %p, effect_group %p: stub\n", iface, effect_group);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateRectangleClip(
        IDCompositionDesktopDevice *iface, IDCompositionRectangleClip **clip)
{
    FIXME("iface %p, clip %p: stub\n", iface, clip);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateAnimation(
        IDCompositionDesktopDevice *iface, IDCompositionAnimation **animation)
{
    FIXME("iface %p, animation %p: stub\n", iface, animation);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateTargetForHwnd(
        IDCompositionDesktopDevice *iface, HWND hwnd, BOOL topmost,
        IDCompositionTarget **target)
{
    struct composition_device *device = impl_from_IDCompositionDesktopDevice(iface);

    TRACE("iface %p, hwnd %p, topmost %d, target %p\n", iface, hwnd, topmost, target);
    return create_target(device, hwnd, topmost, target);
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateSurfaceFromHandle(
        IDCompositionDesktopDevice *iface, HANDLE handle, IUnknown **surface)
{
    FIXME("iface %p, handle %p, surface %p: stub\n", iface, handle, surface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE desktop_device_CreateSurfaceFromHwnd(
        IDCompositionDesktopDevice *iface, HWND hwnd, IUnknown **surface)
{
    FIXME("iface %p, hwnd %p, surface %p: stub\n", iface, hwnd, surface);
    return E_NOTIMPL;
}

static const struct IDCompositionDesktopDeviceVtbl desktop_device_vtbl =
{
    /* IUnknown methods */
    desktop_device_QueryInterface,
    desktop_device_AddRef,
    desktop_device_Release,
    /* IDCompositionDevice2 methods */
    desktop_device_Commit,
    desktop_device_WaitForCommitCompletion,
    desktop_device_GetFrameStatistics,
    desktop_device_CreateVisual,
    desktop_device_CreateSurfaceFactory,
    desktop_device_CreateSurface,
    desktop_device_CreateVirtualSurface,
    desktop_device_CreateTranslateTransform,
    desktop_device_CreateScaleTransform,
    desktop_device_CreateRotateTransform,
    desktop_device_CreateSkewTransform,
    desktop_device_CreateMatrixTransform,
    desktop_device_CreateTransformGroup,
    desktop_device_CreateTranslateTransform3D,
    desktop_device_CreateScaleTransform3D,
    desktop_device_CreateRotateTransform3D,
    desktop_device_CreateMatrixTransform3D,
    desktop_device_CreateTransform3DGroup,
    desktop_device_CreateEffectGroup,
    desktop_device_CreateRectangleClip,
    desktop_device_CreateAnimation,
    /* IDCompositionDesktopDevice methods */
    desktop_device_CreateTargetForHwnd,
    desktop_device_CreateSurfaceFromHandle,
    desktop_device_CreateSurfaceFromHwnd,
};

/*
 * Device factory function and exported DCompositionCreateDevice* APIs
 */

static HRESULT create_device(int version, REFIID iid, void **device)
{
    struct composition_device *object;
    HRESULT hr;

    TRACE("version %d, iid %s, device %p\n", version, debugstr_guid(iid), device);

    if (!device)
        return E_INVALIDARG;

    *device = NULL;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IDCompositionDevice_iface.lpVtbl = &device1_vtbl;
    object->IDCompositionDesktopDevice_iface.lpVtbl = &desktop_device_vtbl;
    object->version = version;
    object->ref = 1;
    InitializeCriticalSection(&object->cs);
    list_init(&object->targets);

    hr = IDCompositionDevice_QueryInterface(&object->IDCompositionDevice_iface, iid, device);
    IDCompositionDevice_Release(&object->IDCompositionDevice_iface);
    return hr;
}

HRESULT WINAPI DCompositionCreateDevice(IDXGIDevice *dxgi_device, REFIID iid, void **device)
{
    TRACE("%p, %s, %p\n", dxgi_device, debugstr_guid(iid), device);
    return create_device(1, iid, device);
}

HRESULT WINAPI DCompositionCreateDevice2(IUnknown *rendering_device, REFIID iid, void **device)
{
    TRACE("%p, %s, %p\n", rendering_device, debugstr_guid(iid), device);
    return create_device(2, iid, device);
}

HRESULT WINAPI DCompositionCreateDevice3(IUnknown *rendering_device, REFIID iid, void **device)
{
    TRACE("%p, %s, %p\n", rendering_device, debugstr_guid(iid), device);
    return create_device(3, iid, device);
}
