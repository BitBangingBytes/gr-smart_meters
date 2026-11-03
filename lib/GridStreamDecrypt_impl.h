/* -*- c++ -*- */
/*
 * Copyright 2026 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_SMART_METERS_GRIDSTREAMDECRYPT_IMPL_H
#define INCLUDED_SMART_METERS_GRIDSTREAMDECRYPT_IMPL_H

#include <smart_meters/GridStreamDecrypt.h>
#include <smart_meters/constants.h>
#include <array>
#include <cstdint>
#include <vector>

namespace gr {
namespace smart_meters {

class GridStreamDecrypt_impl : public GridStreamDecrypt
{
private:
    bool d_lzss;
    bool d_debug;
    bool d_haveKey1{ false };
    bool d_haveKey2{ false };
    std::array<uint8_t, 240> d_rk1{};   // AES-256 expanded round keys (15*16)
    std::array<uint8_t, 240> d_rk2{};

    static bool parse_key(const std::string& hex, std::array<uint8_t, 240>& rk);
    // AES-256 primitives
    static void aes256_expand(const uint8_t key[32], uint8_t rk[240]);
    static void aes256_dec_block(const uint8_t in[16], uint8_t out[16], const uint8_t rk[240]);
    // ECB + CS3 ciphertext-stealing decrypt of a ciphertext span
    static std::vector<uint8_t> ecb(const std::vector<uint8_t>& ct, const uint8_t rk[240]);
    static std::vector<uint8_t> ecb_cts3(const std::vector<uint8_t>& ct, const uint8_t rk[240]);
    static uint16_t crc16_ccitt0(const std::vector<uint8_t>& d);   // poly 0x1021, init 0, no reflect
    // GridStream LZSS decompress (best-effort; see .cc). Returns {} if it cannot run.
    static std::vector<uint8_t> lzss_decompress(const std::vector<uint8_t>& in);

    // Try all keys/modes on one ciphertext; on oracle pass returns plaintext (CRC stripped).
    bool try_decrypt(const std::vector<uint8_t>& ct, std::vector<uint8_t>& pt_out);

    void pdu_handler(pmt::pmt_t pdu);

public:
    GridStreamDecrypt_impl(const std::string& key,
                           const std::string& key2,
                           bool lzss,
                           bool debugEnable);
    ~GridStreamDecrypt_impl() override;
};

} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAMDECRYPT_IMPL_H */
