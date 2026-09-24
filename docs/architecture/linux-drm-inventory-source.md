# Linux DRM PCI inventory source

`LinuxDrmInventorySource` uses libdrm's `drmGetDevices2` API to enumerate DRM
records and reads only records marked `DRM_BUS_PCI` with both PCI bus and PCI
device information present. It does not open DRM nodes, perform ioctls against
a node, or write hardware state. Device enumeration errors remain distinct from
a successful empty inventory. The adapter bounds libdrm's reported inventory,
validates PCI coordinate and ID widths, rejects every duplicate canonical BDF
as ambiguous evidence, and frees allocated libdrm records on every return path.

The result contains AMD vendor/device IDs and a canonical PCI BDF as transient
identity evidence. BDF is not a user-facing label and this adapter makes no
persistence decision. Hardware1 consumers must follow the audited contract's
opaque, typed stable-subject rules and must not substitute an array index or a
display name for identity.

The production library is compiled without the injection API. Tests compile the
same implementation with `ADRENALIN_DRM_INVENTORY_SOURCE_TESTING` to inject raw
enumeration results into the common validation path; the production entry point
continues to call libdrm directly.

## Upstream API references

- libdrm `xf86drm.h` declarations and `drmDevice` PCI fields:
  <https://cgit.freedesktop.org/drm/libdrm/tree/xf86drm.h>
- libdrm `drmGetDevices2` and device-freeing implementation:
  <https://cgit.freedesktop.org/drm/libdrm/tree/xf86drm.c>
