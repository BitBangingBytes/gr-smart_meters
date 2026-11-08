# gr-smart_meters
gr-smart_meters is a GNU Radio out-of-tree module meant to contain various decoders for smart meter manufacturers. 
The initial release contains a decoder for GridStream used by Landis+Gyr.
# Documentation
A guide on what is needed to get this running is available on the RECESSIM Wiki. The decoder is built off of gr-FHSS_Utils by Sandia Labs. https://wiki.recessim.com/view/Gr-smart_meters_Setup_Guide

# Blocks
The GridStream decoder exposes three GNU Radio blocks:
- **`GridStream`** — opcode-keyed parser; frames every packet type and emits rich `gs_*` PDU metadata.
- **`GridStreamDecrypt`** — AES-256-ECB/CTS decrypt + CRC oracle for `0x81` long packets (self-contained AES, no OpenSSL). Needs the network key to produce plaintext.
- **`Deframer`** — multi-burst deframing with per-block `0x81` 10-bit CRC.

The **default branch (`maint-3.10`)** contains all three blocks and the `gs_*` schema — build the default branch; no special branch is required.

# Build dependencies
- **GNU Radio 3.10** + a C++/CMake toolchain (`pybind11`, `boost`, `doxygen`, `pygccxml`).
- The Sandia Labs OOT modules, built against GNU Radio 3.10 at the same install prefix: **`gr-pdu_utils`**, **`gr-fhss_utils`**, and their transitive dep **`gr-timing_utils`**. On **Boost ≥ 1.87** (Arch, Debian 13, Ubuntu 25.10+), upstream `gr-timing_utils` won't compile — use the Asio-ported fork branch [`BitBangingBytes/gr-timing_utils@gridstream-maint-3.10`](https://github.com/BitBangingBytes/gr-timing_utils/tree/gridstream-maint-3.10).
- **Optional:** `gmplot` + `simplekml` — only for the `google_map` / `google_earth` visualization blocks. The core decode blocks import without them.

Build: `cmake -B build -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build -j && sudo cmake --build build -t install && sudo ldconfig`.

For the full receive-only RF monitoring stack that drives these blocks, see [hopstream](https://github.com/BitBangingBytes/hopstream) (tier-1 flowgraph) and [hopstack](https://github.com/BitBangingBytes/hopstack) (umbrella + INSTALL.md).

# Network ID and the per-network CRC
Each GridStream network is identified by a 16-bit **Network ID (NetID)** — the value a utility's
deployment is assigned, and the number the vendor's tooling shows in **decimal**. Frames don't carry the
NetID directly; instead, the per-network CRC seed used to frame and validate every packet is derived
from it:

> **`CRC = CRC-16/CCITT(NetID)`** — polynomial `0x1021`, init `0x0000`, no bit reflection, computed over
> the two NetID bytes most-significant-first. Example: NetID `38100` (`0x94D4`) → CRC `0x5FD6`.

This map is a **bijection over all 16 bits**, so the NetID and its CRC are interchangeable — given either
one you can recover the other. The NetIDs below were recovered from the observed CRCs this way (Oncor's
`38100 → 0x5FD6` is independently confirmed from the meter firmware).

# Known CRC Values

| Location | Utility | NetID (dec) | NetID (hex) | CRC |
|---|---|--:|:--|:--|
| Austin, TX | Austin Energy | 35600 | `0x8B10` | `0xD553` |
| Dallas, TX | CoServ | 37700 | `0x9344` | `0x45F8` |
| Dallas, TX | Oncor | 38100 | `0x94D4` | `0x5FD6` |
| Quebec, CAN | Hydro-Quebec | 42010 | `0xA41A` | `0x62C1` |
| Seattle, WA | Seattle City Light | 34111 | `0x853F` | `0x23D1` |
| Santa Barbara, CA | Southern California Edison | 100 | `0x0064` | `0x2C22` |
| Washington | Puget Sound Energy | 37103 | `0x90EF` | `0x142A` |
| Milwaukee, WI | We Energies | 27000 | `0x6978` | `0x4E2D` |
| Kansas City | Evergy | 8100 | `0x1FA4` | `0xE623` |
| Phoenix, AZ | APS | 64135 | `0xFA87` | `0x1D65` |
| Long Island, NY | PSEG Long Island | 550 | `0x0226` | `0x22C6` |
| Pennsylvania | PPL Electric | 40020 | `0x9C54` | `0x47F7` |
