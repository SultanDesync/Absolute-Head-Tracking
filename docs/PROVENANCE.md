# Provenance

The initial camera/OpenTrack implementation was extracted from AbsoluteHOTAS
5.0.1 at repository commit:

`a0211a6c82a8c13f1264edef52a144d99e5eeba5`

The Dear ImGui D3D12 renderer files were ported from that same tree. Their latest
relevant history entry was:

`02787f5 2026-08-04 Fix RTSS workbench hook regression`

The port intentionally preserves `UIHookInternal.h`, renderer control flow, and
swap-chain/input logic. Differences are limited to standalone branding/message
names, the local compatibility-document path, a head-tracking-specific failure
message, and small public open/close/availability wrappers used by this project.
