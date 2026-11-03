/* -*- c++ -*- */
/*
 * Copyright 2026 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GridStream 0x81 long-packet decrypt. On-air ciphertext =
 *   AES-256-ECB(+CTS)( LZSS(payload) || CRC16 )
 * (M16C firmware: aes_ecb_cts_crypt_node_chain @0x0DBA0E; key = 32-byte network master key
 * at EEPROM page +0xBF; see docs/Long_Packet_0x81_Findings.md / EEPROM_Encryption_Keys.md).
 * The integrity oracle is CRC-16/CCITT(poly 0x1021, init 0, no reflect) over the plaintext == 0,
 * which the firmware itself checks after decrypt. The AES/CTS/CRC here is a direct C++ port of the
 * FIPS-197-validated reference hopalong/research/0x81_block_crc/{aes256,keytest}.py.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "GridStreamDecrypt_impl.h"
#include <gnuradio/io_signature.h>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace gr {
namespace smart_meters {

// ----- AES-256 tables ------------------------------------------------------------------
static const uint8_t SBOX[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16 };

static inline uint8_t xtime(uint8_t a) { return (a & 0x80) ? ((a << 1) ^ 0x1b) : (a << 1); }
static inline uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) { if (b & 1) r ^= a; b >>= 1; a = xtime(a); }
    return r;
}

// AES-256 key schedule -> 60 words = 240 bytes. rk[word*4 + byte].
void GridStreamDecrypt_impl::aes256_expand(const uint8_t key[32], uint8_t rk[240])
{
    uint8_t invb[256]; (void)invb;
    std::memcpy(rk, key, 32);
    uint8_t rcon = 1;
    for (int i = 8; i < 60; i++) {
        uint8_t t[4];
        std::memcpy(t, &rk[(i - 1) * 4], 4);
        if (i % 8 == 0) {
            uint8_t tmp = t[0]; t[0] = t[1]; t[1] = t[2]; t[2] = t[3]; t[3] = tmp;  // RotWord
            for (int j = 0; j < 4; j++) t[j] = SBOX[t[j]];                           // SubWord
            t[0] ^= rcon; rcon = xtime(rcon);
        } else if (i % 8 == 4) {
            for (int j = 0; j < 4; j++) t[j] = SBOX[t[j]];
        }
        for (int j = 0; j < 4; j++) rk[i * 4 + j] = rk[(i - 8) * 4 + j] ^ t[j];
    }
}

// Inverse cipher for one 16-byte block (state column-major: s[r][c] = in[r + 4c]).
void GridStreamDecrypt_impl::aes256_dec_block(const uint8_t in[16], uint8_t out[16], const uint8_t rk[240])
{
    static uint8_t INV[256]; static bool init = false;
    if (!init) { for (int i = 0; i < 256; i++) INV[SBOX[i]] = (uint8_t)i; init = true; }
    uint8_t s[4][4];
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) s[r][c] = in[r + 4 * c];
    auto ark = [&](int rnd) { for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++)
                                  s[r][c] ^= rk[(rnd * 4 + c) * 4 + r]; };
    auto invShift = [&]() {                       // row r rotates RIGHT by r
        for (int r = 1; r < 4; r++) { uint8_t t[4];
            for (int c = 0; c < 4; c++) t[(c + r) & 3] = s[r][c];
            for (int c = 0; c < 4; c++) s[r][c] = t[c]; } };
    auto invSub = [&]() { for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) s[r][c] = INV[s[r][c]]; };
    ark(14);
    for (int rnd = 13; rnd >= 1; rnd--) {
        invShift(); invSub(); ark(rnd);
        for (int c = 0; c < 4; c++) {             // InvMixColumns
            uint8_t a0 = s[0][c], a1 = s[1][c], a2 = s[2][c], a3 = s[3][c];
            s[0][c] = gmul(a0,14) ^ gmul(a1,11) ^ gmul(a2,13) ^ gmul(a3,9);
            s[1][c] = gmul(a0,9)  ^ gmul(a1,14) ^ gmul(a2,11) ^ gmul(a3,13);
            s[2][c] = gmul(a0,13) ^ gmul(a1,9)  ^ gmul(a2,14) ^ gmul(a3,11);
            s[3][c] = gmul(a0,11) ^ gmul(a1,13) ^ gmul(a2,9)  ^ gmul(a3,14);
        }
    }
    invShift(); invSub(); ark(0);
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) out[r + 4 * c] = s[r][c];
}

std::vector<uint8_t> GridStreamDecrypt_impl::ecb(const std::vector<uint8_t>& ct, const uint8_t rk[240])
{
    std::vector<uint8_t> pt;
    size_t nb = ct.size() / 16;
    pt.resize(nb * 16);
    for (size_t i = 0; i < nb; i++) aes256_dec_block(&ct[i * 16], &pt[i * 16], rk);
    return pt;
}

// AES-ECB with CS3 ciphertext stealing for the trailing partial block (keytest.py ecb_cts3).
std::vector<uint8_t> GridStreamDecrypt_impl::ecb_cts3(const std::vector<uint8_t>& ct, const uint8_t rk[240])
{
    size_t n = ct.size();
    if (n < 16) return {};
    size_t d = n % 16;
    if (d == 0) return ecb(ct, rk);
    size_t full = n / 16;                               // full 16-byte blocks present
    std::vector<uint8_t> head(ct.begin(), ct.begin() + (full - 1) * 16);
    std::vector<uint8_t> cn1(ct.begin() + (full - 1) * 16, ct.begin() + full * 16);
    std::vector<uint8_t> cn(ct.begin() + full * 16, ct.end());    // d bytes
    std::vector<uint8_t> pt = ecb(head, rk);
    uint8_t dn1[16]; aes256_dec_block(cn1.data(), dn1, rk);        // = P_n || stolen(16-d)
    std::vector<uint8_t> recon(cn);                               // rebuild E[n-1]
    recon.insert(recon.end(), dn1 + d, dn1 + 16);
    uint8_t p_nm1[16]; aes256_dec_block(recon.data(), p_nm1, rk);
    pt.insert(pt.end(), p_nm1, p_nm1 + 16);
    pt.insert(pt.end(), dn1, dn1 + d);                            // P_n
    return pt;
}

uint16_t GridStreamDecrypt_impl::crc16_ccitt0(const std::vector<uint8_t>& d)
{
    uint16_t c = 0;
    for (uint8_t x : d) { c ^= (uint16_t)x << 8;
        for (int i = 0; i < 8; i++) c = (c & 0x8000) ? ((c << 1) ^ 0x1021) : (c << 1); }
    return c;
}

bool GridStreamDecrypt_impl::parse_key(const std::string& hex, std::array<uint8_t, 240>& rk)
{
    if (hex.size() != 64) return false;
    uint8_t key[32];
    for (int i = 0; i < 32; i++) {
        char b[3] = { hex[i * 2], hex[i * 2 + 1], 0 }; char* e = nullptr;
        long v = strtol(b, &e, 16);
        if (e != b + 2) return false;
        key[i] = (uint8_t)v;
    }
    aes256_expand(key, rk.data());
    return true;
}

// --- GridStream LZSS (best-effort). The firmware uses an LZSS variant (compressor d9399/d91c2/
// d93f0, max block 0xD0) on the plaintext, gated by a block-header flag. The exact framing
// (window size, min-match, flag convention) is NOT yet reverse-engineered, and it CANNOT be
// validated end-to-end without a live-mesh key (none is held). This is a classic LZSS decoder
// (8-flag control byte; flag=1 literal, flag=0 -> 12-bit offset / 4-bit (len-3) back-ref, 4K
// window) provided so the pipeline is complete; treat its output as UNVALIDATED until a key for
// the captured network is obtained. Returns {} if the stream does not decode cleanly. ---
std::vector<uint8_t> GridStreamDecrypt_impl::lzss_decompress(const std::vector<uint8_t>& in)
{
    std::vector<uint8_t> out;
    const size_t N = 4096, THRESHOLD = 3;
    std::vector<uint8_t> win(N, 0);
    size_t r = N - (18 - 1), p = 0;
    while (p < in.size()) {
        uint8_t flags = in[p++];
        for (int b = 0; b < 8 && p < in.size(); b++, flags >>= 1) {
            if (flags & 1) {
                uint8_t c = in[p++]; out.push_back(c); win[r++ & (N - 1)] = c;
            } else {
                if (p + 1 >= in.size()) return out.empty() ? std::vector<uint8_t>{} : out;
                uint8_t b0 = in[p++], b1 = in[p++];
                size_t off = ((size_t)(b1 & 0xF0) << 4) | b0;
                size_t len = (b1 & 0x0F) + THRESHOLD;
                for (size_t k = 0; k < len; k++) { uint8_t c = win[(off + k) & (N - 1)];
                    out.push_back(c); win[r++ & (N - 1)] = c; }
            }
            if (out.size() > 65536) return {};     // runaway guard
        }
    }
    return out;
}

bool GridStreamDecrypt_impl::try_decrypt(const std::vector<uint8_t>& ct, std::vector<uint8_t>& pt_out)
{
    if (ct.size() < 16) return false;
    for (int k = 0; k < 2; k++) {
        if (k == 0 && !d_haveKey1) continue;
        if (k == 1 && !d_haveKey2) continue;
        const uint8_t* rk = (k == 0) ? d_rk1.data() : d_rk2.data();
        std::vector<uint8_t> whole = ecb(ct, rk);
        std::vector<uint8_t> cts = ecb_cts3(ct, rk);
        for (const std::vector<uint8_t>* pt : { &whole, &cts }) {
            if (pt->empty()) continue;
            if (crc16_ccitt0(*pt) == 0) {
                pt_out.assign(pt->begin(), pt->end() >= pt->begin() + 2 ? pt->end() - 2 : pt->end());
                return true;                        // strip the trailing 2-byte CRC16
            }
        }
    }
    return false;
}

GridStreamDecrypt::sptr
GridStreamDecrypt::make(const std::string& key, const std::string& key2, bool lzss, bool debugEnable)
{
    return gnuradio::make_block_sptr<GridStreamDecrypt_impl>(key, key2, lzss, debugEnable);
}

GridStreamDecrypt_impl::GridStreamDecrypt_impl(const std::string& key, const std::string& key2,
                                               bool lzss, bool debugEnable)
    : gr::block("GridStreamDecrypt", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
      d_lzss(lzss), d_debug(debugEnable)
{
    if (!key.empty()) {
        d_haveKey1 = parse_key(key, d_rk1);
        if (!d_haveKey1) GR_LOG_WARN(d_logger, "GridStreamDecrypt: key is not 64 hex chars; ignored");
    }
    if (!key2.empty()) {
        d_haveKey2 = parse_key(key2, d_rk2);
        if (!d_haveKey2) GR_LOG_WARN(d_logger, "GridStreamDecrypt: key2 is not 64 hex chars; ignored");
    }
    message_port_register_in(PMTCONSTSTR__PDU_IN);
    set_msg_handler(PMTCONSTSTR__PDU_IN, [this](pmt::pmt_t pdu) { this->pdu_handler(pdu); });
    message_port_register_out(PMTCONSTSTR__PDU_OUT);
}

GridStreamDecrypt_impl::~GridStreamDecrypt_impl() {}

void GridStreamDecrypt_impl::pdu_handler(pmt::pmt_t pdu)
{
    if (!pmt::is_pdu(pdu)) { GR_LOG_WARN(d_logger, "received non-PDU"); return; }
    pmt::pmt_t meta = pmt::car(pdu);
    pmt::pmt_t vec = pmt::cdr(pdu);

    // Only act on 0x81 encrypted PDUs that the GridStream parser tagged. Everything else passes
    // through verbatim, so this block can sit inline on the full PDU stream.
    bool enc = pmt::dict_has_key(meta, pmt::mp("gs_enc"));
    bool haveCt = pmt::dict_has_key(meta, pmt::mp("gs_ct_off")) &&
                  pmt::dict_has_key(meta, pmt::mp("gs_payload"));
    if (enc && haveCt && (d_haveKey1 || d_haveKey2)) {
        std::vector<uint8_t> payload =
            pmt::u8vector_elements(pmt::dict_ref(meta, pmt::mp("gs_payload"), pmt::PMT_NIL));
        long off = pmt::to_long(pmt::dict_ref(meta, pmt::mp("gs_ct_off"), pmt::from_long(0)));
        std::vector<uint8_t> ct;
        if (off >= 0 && (size_t)off <= payload.size())
            ct.assign(payload.begin() + off, payload.end());

        std::vector<uint8_t> pt;
        bool ok = try_decrypt(ct, pt);
        if (ok) {
            meta = pmt::dict_add(meta, pmt::mp("gs_enc"), pmt::mp(std::string("decrypted")));
            meta = pmt::dict_add(meta, pmt::mp("gs_plaintext"), pmt::init_u8vector(pt.size(), pt));
            if (d_lzss) {
                std::vector<uint8_t> dec = lzss_decompress(pt);
                if (!dec.empty())
                    meta = pmt::dict_add(meta, pmt::mp("gs_decompressed"),
                                         pmt::init_u8vector(dec.size(), dec));
            }
            if (d_debug)
                std::cout << "[GridStreamDecrypt] DECRYPTED " << pt.size() << " plaintext bytes\n";
        } else {
            meta = pmt::dict_add(meta, pmt::mp("gs_enc"), pmt::mp(std::string("decrypt_failed")));
            if (d_debug)
                std::cout << "[GridStreamDecrypt] key did not satisfy CRC oracle (" << ct.size()
                          << " ct bytes)\n";
        }
    }
    message_port_pub(PMTCONSTSTR__PDU_OUT, pmt::cons(meta, vec));
}

} // namespace smart_meters
} // namespace gr
