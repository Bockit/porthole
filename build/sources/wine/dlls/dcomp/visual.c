/*
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

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "dcomp_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dcomp);

static HRESULT STDMETHODCALLTYPE visual2_QueryInterface(IDCompositionVisual2 *iface, REFIID iid,
        void **out)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);

    TRACE("iface %p, iid %s, out %p\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IDCompositionVisual)
            || (visual->version >= 2 && IsEqualGUID(iid, &IID_IDCompositionVisual2)))
    {
        IUnknown_AddRef(&visual->IDCompositionVisual2_iface);
        *out = &visual->IDCompositionVisual2_iface;
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE visual2_AddRef(IDCompositionVisual2 *iface)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);
    ULONG ref = InterlockedIncrement(&visual->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE visual2_Release(IDCompositionVisual2 *iface)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);
    ULONG ref = InterlockedDecrement(&visual->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);

    if (!ref)
    {
        struct visual_child *child, *next;

        LIST_FOR_EACH_ENTRY_SAFE(child, next, &visual->children, struct visual_child, entry)
        {
            IDCompositionVisual2_Release(child->visual);
            list_remove(&child->entry);
            free(child);
        }
        if (visual->content)
            IUnknown_Release(visual->content);
        free(visual);
    }

    return ref;
}

static HRESULT STDMETHODCALLTYPE visual2_SetOffsetXAnimation(IDCompositionVisual2 *iface,
        IDCompositionAnimation *animation)
{
    TRACE("iface %p, animation %p\n", iface, animation);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetOffsetX(IDCompositionVisual2 *iface, float offset_x)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);

    TRACE("iface %p, offset_x %f\n", iface, offset_x);
    visual->offset_x = offset_x;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetOffsetYAnimation(IDCompositionVisual2 *iface,
        IDCompositionAnimation *animation)
{
    TRACE("iface %p, animation %p\n", iface, animation);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetOffsetY(IDCompositionVisual2 *iface, float offset_y)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);

    TRACE("iface %p, offset_y %f\n", iface, offset_y);
    visual->offset_y = offset_y;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetTransform(IDCompositionVisual2 *iface,
        const D2D_MATRIX_3X2_F *matrix)
{
    TRACE("iface %p, matrix %p\n", iface, matrix);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetTransformObject(IDCompositionVisual2 *iface,
        IDCompositionTransform *transform)
{
    TRACE("iface %p, transform %p\n", iface, transform);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetTransformParent(IDCompositionVisual2 *iface,
        IDCompositionVisual *visual)
{
    TRACE("iface %p, visual %p\n", iface, visual);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetEffect(IDCompositionVisual2 *iface,
        IDCompositionEffect *effect)
{
    TRACE("iface %p, effect %p\n", iface, effect);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetBitmapInterpolationMode(IDCompositionVisual2 *iface,
        enum DCOMPOSITION_BITMAP_INTERPOLATION_MODE interpolation_mode)
{
    TRACE("iface %p, interpolation_mode %d\n", iface, interpolation_mode);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetBorderMode(IDCompositionVisual2 *iface,
        enum DCOMPOSITION_BORDER_MODE border_mode)
{
    TRACE("iface %p, border_mode %d\n", iface, border_mode);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetClip(IDCompositionVisual2 *iface, const D2D_RECT_F *rect)
{
    TRACE("iface %p, rect %p\n", iface, rect);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetClipObject(IDCompositionVisual2 *iface,
        IDCompositionClip *clip)
{
    TRACE("iface %p, clip %p\n", iface, clip);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetContent(IDCompositionVisual2 *iface, IUnknown *content)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);

    TRACE("iface %p, content %p\n", iface, content);

    if (visual->content)
        IUnknown_Release(visual->content);

    visual->content = content;
    if (content)
        IUnknown_AddRef(content);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_AddVisual(IDCompositionVisual2 *iface,
        IDCompositionVisual *child_visual, BOOL insert_above, IDCompositionVisual *reference_visual)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);
    struct visual_child *child;

    TRACE("iface %p, child %p, insert_above %d, reference %p\n", iface, child_visual,
            insert_above, reference_visual);

    child = calloc(1, sizeof(*child));
    if (!child)
        return E_OUTOFMEMORY;

    child->visual = (IDCompositionVisual2 *)child_visual;
    IDCompositionVisual2_AddRef(child->visual);
    list_add_tail(&visual->children, &child->entry);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_RemoveVisual(IDCompositionVisual2 *iface,
        IDCompositionVisual *child_visual)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);
    struct visual_child *child, *next;

    TRACE("iface %p, child %p\n", iface, child_visual);

    LIST_FOR_EACH_ENTRY_SAFE(child, next, &visual->children, struct visual_child, entry)
    {
        if (child->visual == (IDCompositionVisual2 *)child_visual)
        {
            IDCompositionVisual2_Release(child->visual);
            list_remove(&child->entry);
            free(child);
            return S_OK;
        }
    }
    return E_INVALIDARG;
}

static HRESULT STDMETHODCALLTYPE visual2_RemoveAllVisuals(IDCompositionVisual2 *iface)
{
    struct composition_visual *visual = impl_from_IDCompositionVisual2(iface);
    struct visual_child *child, *next;

    TRACE("iface %p\n", iface);

    LIST_FOR_EACH_ENTRY_SAFE(child, next, &visual->children, struct visual_child, entry)
    {
        IDCompositionVisual2_Release(child->visual);
        list_remove(&child->entry);
        free(child);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetCompositeMode(IDCompositionVisual2 *iface,
        enum DCOMPOSITION_COMPOSITE_MODE composite_mode)
{
    TRACE("iface %p, composite_mode %d\n", iface, composite_mode);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetOpacityMode(IDCompositionVisual2 *iface,
        enum DCOMPOSITION_OPACITY_MODE opacity_mode)
{
    TRACE("iface %p, opacity_mode %d\n", iface, opacity_mode);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE visual2_SetBackFaceVisibility(IDCompositionVisual2 *iface,
        enum DCOMPOSITION_BACKFACE_VISIBILITY visibility)
{
    TRACE("iface %p, visibility %d\n", iface, visibility);
    return S_OK;
}

static const struct IDCompositionVisual2Vtbl visual2_vtbl =
{
    /* IUnknown methods */
    visual2_QueryInterface,
    visual2_AddRef,
    visual2_Release,
    /* IDCompositionVisual methods */
    visual2_SetOffsetXAnimation,
    visual2_SetOffsetX,
    visual2_SetOffsetYAnimation,
    visual2_SetOffsetY,
    visual2_SetTransformObject,
    visual2_SetTransform,
    visual2_SetTransformParent,
    visual2_SetEffect,
    visual2_SetBitmapInterpolationMode,
    visual2_SetBorderMode,
    visual2_SetClipObject,
    visual2_SetClip,
    visual2_SetContent,
    visual2_AddVisual,
    visual2_RemoveVisual,
    visual2_RemoveAllVisuals,
    visual2_SetCompositeMode,
    /* IDCompositionVisual2 methods */
    visual2_SetOpacityMode,
    visual2_SetBackFaceVisibility,
};

HRESULT create_visual(int version, REFIID iid, void **new_visual)
{
    struct composition_visual *visual;
    HRESULT hr;

    if (!new_visual)
        return E_INVALIDARG;

    visual = calloc(1, sizeof(*visual));
    if (!visual)
        return E_OUTOFMEMORY;

    visual->IDCompositionVisual2_iface.lpVtbl = &visual2_vtbl;
    visual->version = version;
    visual->ref = 1;
    list_init(&visual->children);
    hr = IUnknown_QueryInterface(&visual->IDCompositionVisual2_iface, iid, new_visual);
    IUnknown_Release(&visual->IDCompositionVisual2_iface);
    return hr;
}
