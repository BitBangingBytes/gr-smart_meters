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

# Known CRC Values
Austin, TX.   |   Austin Energy       |   CRC = 0xD553

Dallas, TX    |   CoServ              |   CRC = 0x45F8

Dallas, TX    |   Oncor               |   CRC = 0x5FD6

Quebec, CAN   |   Hydro-Quebec        |   CRC = 0x62C1

Seattle, WA   |   Seattle City Light  |   CRC = 0x23D1

Santa Barbara, CA   |   Southern California Edison   |   CRC = 0x2C22

Washington   |   Puget Sound Energy   |   CRC = 0x142A

Milwaukee, WI   |   We Energies   |   CRC = 0x4E2D

Kansas City  |   Evergy               |   CRC = 0xE623

Phoenix, AZ  |   APS                  |   CRC = 0x1D65

Long Island, NY  |  PSEG Long Island   |   CRC = 0x22C6

Pennsylvania  |  PPL Electric  |  CRC = 0x47F7
