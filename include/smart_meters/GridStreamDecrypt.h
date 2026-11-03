/* -*- c++ -*- */
/*
 * Copyright 2026 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_SMART_METERS_GRIDSTREAMDECRYPT_H
#define INCLUDED_SMART_METERS_GRIDSTREAMDECRYPT_H

#include <gnuradio/block.h>
#include <smart_meters/api.h>
#include <string>

namespace gr {
namespace smart_meters {

/*!
 * \brief Decrypt GridStream 0x81 long-packet payloads (AES-256-ECB + CTS), with the
 *        meter firmware's CRC-16==0 integrity oracle, and optional LZSS decompress.
 * \ingroup smart_meters
 *
 * Consumes PDUs (typically from the GridStream parser block). A PDU tagged
 * gs_enc="encrypted" (0x81 block-mode) with gs_ct_off/gs_ct_len is decrypted with the
 * configured key(s); on an oracle pass the PDU is re-tagged gs_enc="decrypted", the
 * recovered plaintext is attached as gs_plaintext, and (if enabled) gs_decompressed.
 * All other PDUs pass through unchanged. No key -> frames stay tagged "encrypted".
 */
class SMART_METERS_API GridStreamDecrypt : virtual public gr::block
{
public:
    typedef std::shared_ptr<GridStreamDecrypt> sptr;

    /*!
     * \param key   32-byte AES-256 network key as 64 hex chars ("" = none).
     * \param key2  optional second key to try (64 hex, "" = none).
     * \param lzss  attempt LZSS decompress of the decrypted plaintext.
     * \param debugEnable  print a line per decrypted/failed PDU.
     */
    static sptr make(const std::string& key,
                     const std::string& key2,
                     bool lzss,
                     bool debugEnable);
};

} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAMDECRYPT_H */
