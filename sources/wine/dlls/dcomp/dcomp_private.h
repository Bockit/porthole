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
#ifndef __WINE_DCOMP_PRIVATE_H
#define __WINE_DCOMP_PRIVATE_H

#include "dcomp.h"
#include "wine/list.h"

/* IDCompositionDevice3 is not in the CX26 IDL, define manually */
DEFINE_GUID(IID_IDCompositionDevice3, 0x0987cb06, 0xf916, 0x48bf, 0x8d,0x35, 0xce,0x76,0x41,0x78,0x1b,0xd9);

struct composition_device
{
    IDCompositionDevice IDCompositionDevice_iface;
    IDCompositionDesktopDevice IDCompositionDesktopDevice_iface;
    CRITICAL_SECTION cs;
    struct list targets;
    HANDLE thread;
    BOOL thread_exited;
    int version;
    LONG ref;
};

struct composition_target
{
    IDCompositionTarget IDCompositionTarget_iface;
    IDCompositionDevice *device;
    IDCompositionVisual *root;
    BOOL topmost;
    HWND hwnd;
    struct list entry;
    LONG ref;
};

struct composition_visual
{
    IDCompositionVisual2 IDCompositionVisual2_iface;
    IUnknown *content;
    struct list children;
    struct list entry;
    BOOL is_root;
    float offset_x;
    float offset_y;
    int version;
    LONG ref;
};

struct visual_child
{
    IDCompositionVisual2 *visual;
    struct list entry;
};

static inline struct composition_device *impl_from_IDCompositionDevice(IDCompositionDevice *iface)
{
    return CONTAINING_RECORD(iface, struct composition_device, IDCompositionDevice_iface);
}

static inline struct composition_device *impl_from_IDCompositionDesktopDevice(IDCompositionDesktopDevice *iface)
{
    return CONTAINING_RECORD(iface, struct composition_device, IDCompositionDesktopDevice_iface);
}

static inline struct composition_target *impl_from_IDCompositionTarget(IDCompositionTarget *iface)
{
    return CONTAINING_RECORD(iface, struct composition_target, IDCompositionTarget_iface);
}

static inline struct composition_visual *impl_from_IDCompositionVisual(IDCompositionVisual *iface)
{
    return CONTAINING_RECORD(iface, struct composition_visual, IDCompositionVisual2_iface);
}

static inline struct composition_visual *impl_from_IDCompositionVisual2(IDCompositionVisual2 *iface)
{
    return CONTAINING_RECORD(iface, struct composition_visual, IDCompositionVisual2_iface);
}

HRESULT create_target(struct composition_device *device, HWND hwnd, BOOL topmost, IDCompositionTarget **target);
HRESULT create_visual(int version, REFIID iid, void **visual);

#endif /* __WINE_DCOMP_PRIVATE_H */
