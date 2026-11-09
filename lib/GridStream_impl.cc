/* -*- c++ -*- */
/*
 * Copyright 2021, 2022 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "GridStream_impl.h"
#include <gnuradio/io_signature.h>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

namespace gr {
namespace smart_meters {

GridStream::sptr GridStream::make(
					bool crcEnable,
					bool debugEnable,
					bool crcColor,
					bool timestampEnable,
					bool epochEnable,
					bool frequencyEnable,
					bool baudrateEnable,
					uint16_t crcInitialValue,
					uint32_t meterLanSrcID,
					uint32_t meterLanDstID,
					uint8_t  packetTypeFilter,
					uint16_t packetLengthFilter,
					uint8_t  crcCorrectBits)
{
    return gnuradio::make_block_sptr<GridStream_impl>(
        crcEnable, debugEnable, crcColor, timestampEnable, epochEnable, frequencyEnable, baudrateEnable,
        crcInitialValue, meterLanSrcID, meterLanDstID, packetTypeFilter, packetLengthFilter,
        crcCorrectBits);
}


/*
 * The private constructor
 */
GridStream_impl::GridStream_impl(
					bool crcEnable,
					bool debugEnable,
					bool crcColor,
					bool timestampEnable,
					bool epochEnable,
					bool frequencyEnable,
					bool baudrateEnable,
					uint16_t crcInitialValue,
					uint32_t meterLanSrcID,
					uint32_t meterLanDstID,
					uint8_t  packetTypeFilter,
					uint16_t packetLengthFilter,
					uint8_t  crcCorrectBits)
    : gr::block(
          "GridStream", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
      d_crcEnable(crcEnable),
      d_debugEnable(debugEnable),
      d_crcColor(crcColor),
      d_timestampEnable(timestampEnable),
      d_epochEnable(epochEnable),
      d_frequencyEnable(frequencyEnable),
      d_baudrateEnable(baudrateEnable),
      d_crcInitialValue(crcInitialValue),
      d_meterLanSrcID(meterLanSrcID),
      d_meterLanDstID(meterLanDstID),
      d_packetTypeFilter(packetTypeFilter),
      d_packetLengthFilter(packetLengthFilter),
      d_crcCorrectBits(crcCorrectBits)
{
    message_port_register_in(PMTCONSTSTR__PDU_IN);
    set_msg_handler(PMTCONSTSTR__PDU_IN,
                    [this](pmt::pmt_t pdu) { this->pdu_handler(pdu); });
    message_port_register_out(PMTCONSTSTR__PDU_OUT);
}

/*
 * Our virtual destructor.
 */
GridStream_impl::~GridStream_impl() {}

template <typename T>
std::string int_to_hex(T i)
{
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << std::uppercase
           << i;
    return stream.str();
}

template <typename T>
std::string char_to_hex(T i)
{
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << i;
    return stream.str();
}

std::string time_in_HH_MM_SS_MMM()
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

uint16_t
GridStream_impl::crc16(uint16_t crc, const std::vector<uint8_t>& data, size_t packet_len, size_t header_len)
{
    uint16_t Poly = 0x1021;
    const int crc_len = 2;
    uint16_t i = header_len; // Skip over header/packet length [eg. 00,FF,2A,55,xx,(xx)]
    int size = packet_len - crc_len;
    while (size--) {
        crc ^= data[i] << 8;
        i++;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 0x8000 ? (crc << 1) ^ Poly : crc << 1;
    }
    return crc;
}

// One-bit CRC repair. data[header_len .. header_len+packet_len) = body + 2 CRC bytes. Returns the
// PDU bit index (MSB-first) of the single flip that makes the CRC match, applying it, or -1 when
// no flip or more than one flip does (then nothing is changed).
int
GridStream_impl::fix_single_bit(std::vector<uint8_t>& data, size_t header_len, size_t packet_len)
{
    if (packet_len < 3 || header_len + packet_len > data.size())
        return -1;
    const size_t first = header_len * 8, end = (header_len + packet_len) * 8;
    auto ok = [&]() {
        uint16_t rx = (uint16_t(data[header_len + packet_len - 2]) << 8) |
                      data[header_len + packet_len - 1];
        return crc16(d_crcInitialValue, data, packet_len, header_len) == rx;
    };
    int hit = -1;
    for (size_t b = first; b < end; b++) {
        data[b / 8] ^= uint8_t(0x80 >> (b % 8));
        bool match = ok();
        data[b / 8] ^= uint8_t(0x80 >> (b % 8));
        if (match) {
            if (hit >= 0)
                return -1;          // ambiguous: refuse
            hit = int(b);
        }
    }
    if (hit >= 0)
        data[hit / 8] ^= uint8_t(0x80 >> (hit % 8));
    return hit;
}

static uint16_t reflect16(uint16_t x)
{
    uint16_t r = 0;
    for (int i = 0; i < 16; i++) {
        r = (r << 1) | (x & 1);
        x >>= 1;
    }
    return r;
}

// 0x81 block-mode CRC10 (M16C RX state 0xE, df066/df096). The M16C HW CRC peripheral (reflected
// CCITT fed bit-reversed bytes, seed reflect16(init)) equals reflect16(MSB-first CRC16(init)), so
// the existing crc16() is reused. Form 1 = CRC over data; form 2 (what captured senders use) =
// CRC over data + (hdr & 0xF8). Check: (crc & 0x3FF) == ((hdr & 3) << 8) | lo.
bool GridStream_impl::block_crc_ok(uint8_t hdr, const std::vector<uint8_t>& data, size_t from,
                                   size_t n, uint8_t lo)
{
    if (from + n > data.size())
        return false;
    const uint16_t want = (uint16_t)(((hdr & 3) << 8) | lo);
    std::vector<uint8_t> v(data.begin() + from, data.begin() + from + n);
    uint16_t c1 = crc16(d_crcInitialValue, v, v.size() + 2, 0);
    if ((reflect16(c1) & 0x3FF) == want)
        return true;                                   // form 1
    v.push_back(hdr & 0xF8);
    uint16_t c2 = crc16(d_crcInitialValue, v, v.size() + 2, 0);
    return (reflect16(c2) & 0x3FF) == want;            // form 2
}

void GridStream_impl::pdu_handler(pmt::pmt_t pdu)
{
    pmt::pmt_t meta = pmt::car(pdu);
    pmt::pmt_t vector_data = pmt::cdr(pdu);

    // make sure PDU data is formed properly
    if (!(pmt::is_pdu(pdu))) {
        GR_LOG_WARN(d_logger, "received unexpected PMT (non-pdu)");
        return;
    }
    size_t v_data_len = pmt::length(pmt::cdr(pdu));
    // Packet not large enough, probably noise
    if (v_data_len < 6) {
        return;
    }
    std::vector<uint8_t> data = pmt::u8vector_elements(vector_data);   // mutable: CRC repair

    // ---- 0x81 block-mode long packet (docs/0x81_Block_Mode_CRC.md) -----------------------
    // Each burst: [00|80] FF hdr data[L] lo [04];  hdr = (k<<3) | (last<<2) | crc[9:8];
    // L = 80 except the last block (its length comes from the handshake, so it ends at lo =
    // the last complete byte; the 04 tail is almost never on air).
    const bool block_mode = (data[2] != 0x2A) && !(data[2] & 0x80) && ((data[2] >> 3) == 0) &&
                            (data[3] == 0x81);
    int blk_count = 0, blk_ok = 0;
    bool blk_complete = false;
    std::vector<uint8_t> blk_results, blk_payload;
    if (block_mode) {
        const size_t BLK = 80;
        // Block boundaries come from the FIRMWARE LAYOUT ONLY, never from searching for "00 FF" or
        // from the Deframer's GridStream_Bursts offsets: with no inter-burst gap the Deframer decodes
        // straight through the re-sync, so a boundary is bit-identical to a 00 FF pair inside
        // ciphertext. Trusting those offsets fabricates phantom blocks (a 96 B PDU -> 160 B payload).
        // After a non-last block (hdr, 80 data, lo) the next block begins right away, or after one 04
        // tail byte (TX state 0x18). An 80-byte-block transmission with no dropped burst needs nothing
        // more; a genuinely dropped burst just fails the block CRC, which is the correct outcome.
        auto is_sync = [&](size_t at) {
            return at + 2 < data.size() && (data[at] == 0x00 || data[at] == 0x80) &&
                   data[at + 1] == 0xFF;
        };
        std::vector<uint32_t> starts;
        size_t st = 0;
        while (st + 3 < data.size()) {
            starts.push_back((uint32_t)st);
            if (data[st + 2] & 0x04) break;                  // last block
            size_t nx = st + 3 + BLK + 1;                    // just after lo
            if (is_sync(nx)) {
                st = nx;
            } else if (nx < data.size() && data[nx] == 0x04 && is_sync(nx + 1)) {
                st = nx + 1;                                 // 04 tail present
            } else {
                break;
            }
        }
        for (size_t i = 0; i < starts.size(); i++) {
            size_t s0 = starts[i];
            size_t e0 = (i + 1 < starts.size()) ? starts[i + 1] : data.size();
            if (s0 + 4 > e0 || e0 > data.size()) break;
            uint8_t hdr = data[s0 + 2];
            if ((hdr & 0x80) || (size_t)(hdr >> 3) != i) break;   // RX rejects bit7; order
            bool last = hdr & 0x04;
            bool ok = false;
            size_t dlen = 0;
            if (!last) {
                if (s0 + 3 + BLK >= e0) {        // truncated: the 80 data bytes + lo are not all here
                    blk_count++;
                    blk_results.push_back(0);
                    break;                       // no CRC, no payload (matches the Python reference)
                }
                dlen = BLK;
                ok = block_crc_ok(hdr, data, s0 + 3, BLK, data[s0 + 3 + BLK]);
            } else {
                dlen = e0 - 1 - (s0 + 3);
                ok = block_crc_ok(hdr, data, s0 + 3, dlen, data[e0 - 1]);
                if (!ok && data[e0 - 1] == 0x04 && e0 - 2 > s0 + 3) {   // tail was captured
                    dlen = e0 - 2 - (s0 + 3);
                    ok = block_crc_ok(hdr, data, s0 + 3, dlen, data[e0 - 2]);
                }
            }
            blk_count++;
            blk_ok += ok ? 1 : 0;
            blk_results.push_back(ok ? 1 : 0);
            if (s0 + 3 + dlen <= data.size())
                blk_payload.insert(blk_payload.end(), data.begin() + s0 + 3,
                                   data.begin() + s0 + 3 + dlen);
            if (last) {
                blk_complete = true;
                break;
            }
        }
    }

    // Packet decode 00,FF,message_type,packet_type,packet_len
	int header_len = { 0 };
    int packet_type = data[3];
    int packet_len = { 0 };
    if (packet_type == 0xD2) {
        header_len = 5;
        packet_len = data[4];
    } else if ((packet_type == 0x55) || (packet_type == 0xD5)) {
        header_len = 6;
        packet_len = data[5];
    } else {
        packet_len = v_data_len;
    }

    const int min_packet_size = { 4 };
    // Packet self reports too small
    if (packet_len < min_packet_size) {
        return;
    }

    int decoded_packet_size = header_len + packet_len;
    bool malformed_packet = false;
    // Packet self reports larger than data we received
    if (decoded_packet_size > data.size()) {
        malformed_packet = true;
        decoded_packet_size = data.size();
    }

    // ---- CRC applicability (reverse-engineering finding, 2026-10) ------------------
    // Only base-0x55 frames (0x55 broadcast beacon, 0xD5 addressed) carry a link CRC-16
    // that the meter's receiver actually checks (M16C firmware RX state 5). Routed
    // point-to-point frames (0xD2/0x52) and format-0x17 frames (0x25/0xA5) are handled by
    // RX state 6, which SKIPS the CRC step entirely -- no CRC is appended on TX or verified
    // on RX. A CRC compare on those types is meaningless, so we must NOT flag them BAD (and
    // must not drop them): we report CRC state "NA" and fall back to a structural check.
    bool crc_applies = (packet_type == 0x55) || (packet_type == 0xD5) || block_mode;

    int receivedCRC = { 0 };
    uint16_t calculatedCRC = { 1 };
    int crc_high_byte = { 2 };
    int crc_low_byte = { 1 };
    // crc_state: 0 = NA (type not CRC-protected), 1 = OK (match), 2 = BAD (mismatch/unverifiable).
    int crc_state = 0;
    int crc_fixed_bit = -1;   // >= 0: bit repaired by the one-bit CRC correction
    // struct_ok: structural validity used to admit CRC-NA types we cannot CRC-check but can
    // sanity-check. For CRC-checkable types the gate uses crc_state, not struct_ok.
    bool struct_ok = !malformed_packet;
    if (crc_applies) {
        // 0x55/0xD5 are CRC-protected: always OK or BAD, never NA. A malformed/truncated
        // frame (CRC bytes not captured) could not be verified, so it did not pass -> BAD.
        if (block_mode) {
            // 0x81: OK only if every block passes AND the last block was received.
            crc_state = (blk_count > 0 && blk_ok == blk_count && blk_complete) ? 1 : 2;
        } else {
        if (!malformed_packet) {
            receivedCRC = data[header_len + packet_len - crc_low_byte] |
                          data[header_len + packet_len - crc_high_byte] << 8;
            calculatedCRC = GridStream_impl::crc16(d_crcInitialValue, data, packet_len, header_len);
        }
        crc_state = (!malformed_packet && (receivedCRC == calculatedCRC)) ? 1 : 2;
        if (crc_state == 2 && !malformed_packet && d_crcCorrectBits >= 1) {
            crc_fixed_bit = fix_single_bit(data, (size_t)header_len, (size_t)packet_len);
            if (crc_fixed_bit >= 0)
                crc_state = 1;
        }
        }
    } else if (!malformed_packet) {
        crc_state = 0;  // NA: not CRC-protected by the link layer
        if (packet_type == 0xD2) {
            // Routed D2 self-reported length (firmware RX state 4) is bounded 4..0xF4.
            struct_ok = (packet_len >= 4) && (packet_len <= 0xF4);
        } else {
            // 0x25/0xA5, 0x81, unknown: layout not fully modelled; admit on framing only.
            struct_ok = true;
        }
    }

    // ============================ FIELD EXTRACTION (redesigned 2026-11) =====================
    // Parse EVERY frame type into PDU metadata so this block is the single parser any downstream
    // system consumes. Address width is keyed on the L2 opcode (ProtocolType|FrameType, from the
    // L+G collector CLppLink.FillHeader), NOT the packet_type 0x80 "routed" bit -- that bit does
    // not change address width, which is the bug that misread 0xD5 Sync beacons as 4-byte LAN and
    // produced phantom 'ffff50..' ids. Link header: [opcode][DstLAN:4][SrcLAN:4][FrameId:1].
    // The Sync beacon (ProtocolType 0x30) is the ONLY link frame with broadcast dst + 6-byte WAN
    // source + the real LAN identity at [26:30]; it appears under BOTH packet_type 0x55 and 0xD5.
    //
    //   PDU metadata schema -- OLD key (pre-2026-11)  ->  NEW key:
    //     Gridstream_LanSrcID   -> gs_txlan      (per-hop: LAN of the node transmitting THIS frame/hop)
    //     Gridstream_LanDstID   -> gs_rxlan      (per-hop: LAN of the immediate receiver; absent if broadcast)
    //     Gridstream_WanSrcID   -> gs_txwan      (per-hop: transmitter's own 6-byte WAN; end-to-end addrs = gs_{src,dst}{lan,wan})
    //     Gridstream_Uptime     -> gs_uptime
    //     Gridstream_Freq       -> gs_freq
    //     Gridstream_CRCState   -> gs_crc        ("OK"/"BAD"/"NA")
    //     Gridstream_BlockMode  -> gs_blockmode ; _Blocks -> gs_blocks ; _BlocksOK -> gs_blocks_ok
    //     Gridstream_Complete   -> gs_complete  ; _BlockCRC -> gs_block_crc ; _Payload -> gs_payload
    //   New fields (no old equivalent): gs_ver gs_mtype gs_ptype gs_len gs_opcode gs_proto gs_ftype
    //     gs_frameid gs_class gs_bcast gs_lat gs_lon gs_color gs_regts gs_statusseq gs_statusflags
    //     gs_hopms gs_d2op gs_d2advert
    // =======================================================================================
    uint32_t receivedMeterLanSrcID{ 0xFFFFFFFF };
    uint32_t receivedMeterLanDstID{ 0xFFFFFFFF };
    int  upTime{ 0 };
    std::string gsSrc, gsDst, gsWan, gsProto, gsFType, gsClass;
    int  gsOpcode{ -1 }, gsFrameId{ -1 }, gsColor{ -1 };
    int  gsRegTs{ 0 }, gsStatusSeq{ -1 }, gsStatusFlags{ -1 }, gsHopMs{ -1 }, gsD2Op{ -1 };
    int  gsCtOff{ -1 }, gsCtLen{ -1 };             // 0x81 ciphertext span within gs_payload
    std::string gsDstWan, gsDstLan, gsSrcWan, gsSrcLan;  // 0x81 end-to-end routing-header addrs (network layer)
    bool gsBcast{ false }, gsD2Advert{ false }, gsHaveGps{ false }, gsEnc{ false };
    double gsLat{ 0 }, gsLon{ 0 };
    int  gsVer = (data[0] == 0x80) ? 5 : (data[0] == 0x00 ? 4 : 0);

    auto hexn = [&](size_t o, size_t n) -> std::string {
        std::string s; if (o + n > data.size()) return s;
        for (size_t i = 0; i < n; i++) s += char_to_hex(int(data[o + i])); return s; };
    auto be32 = [&](size_t o) -> uint32_t {
        if (o + 4 > data.size()) return 0u;
        return (uint32_t(data[o]) << 24) | (uint32_t(data[o + 1]) << 16) |
               (uint32_t(data[o + 2]) << 8) | uint32_t(data[o + 3]); };
    auto u32id = [](const std::string& h) -> uint32_t {
        return (h.size() == 8) ? (uint32_t)strtoul(h.c_str(), nullptr, 16) : 0xFFFFFFFFu; };
    auto wanGps = [&](size_t o) {                  // decode 6-byte WAN at offset o -> hex + lat/lon
        if (o + 6 > data.size()) return;
        uint64_t V = 0; for (int i = 0; i < 6; i++) V = (V << 8) | data[o + i];
        gsWan = hexn(o, 6);
        if (((V >> 40) & 0xFF) == 0xFE) return;    // 0xFE guard = Mobile / non-GPS (LAN-derived)
        const double SCALE = 360.0 / 4194304.0;    // 360 / 2^22
        gsLat = 90.0 - double((V >> 27) & 0x1FFFFF) * SCALE;
        gsLon = double((V >> 5) & 0x3FFFFF) * SCALE; if (gsLon > 180.0) gsLon -= 360.0;
        gsColor = int(V & 0x1F); gsHaveGps = true; };
    auto protoName = [](int p) -> std::string {    // vendor ProtocolType (opcode & 0xFC)
        switch (p) { case 0x00: return "Null"; case 0x08: return "Test"; case 0x20: return "Tickle";
            case 0x28: return "Data";              // meter-firmware Data base (this collector build uses 0xC0)
            case 0x30: return "Sync"; case 0x40: return "Timestamp"; case 0x44: return "TimestampInq";
            case 0x50: return "LinkInfo"; case 0x54: return "LinkInfoInq"; case 0xC0: return "Data";
            default: { char b[8]; snprintf(b, sizeof b, "0x%02X", p & 0xFF); return std::string(b); } } };
    static const char* FT[4] = { "DataNoAck", "DataAck", "Ack", "AckAck" };

    if ((packet_type == 0x55 || packet_type == 0xD5) && crc_state == 1) {
        int opcode = (data.size() > 6) ? data[6] : -1;
        gsOpcode = opcode;
        int proto = opcode & 0xFC, ftype = opcode & 0x03;
        gsProto = protoName(proto); gsFType = FT[ftype];
        if (proto == 0x30) {                       // SYNC beacon: broadcast dst + WAN src + LAN@26
            gsBcast = true;
            wanGps(13);                            // 6-byte WAN -> GPS
            if (data.size() >= 24) upTime = (int)be32(20);
            gsSrc = hexn(26, 4);                   // real transmitter LAN identity
            receivedMeterLanSrcID = u32id(gsSrc);
            gsClass = (packet_len == 0x23) ? "beacon" : "beacon_other";
        } else {                                   // 4-byte LAN link frame
            gsDst = hexn(7, 4); gsSrc = hexn(11, 4);
            if (data.size() > 15) gsFrameId = data[15];
            receivedMeterLanSrcID = u32id(gsSrc);
            receivedMeterLanDstID = u32id(gsDst);
            if (packet_len == 0x47 && data.size() >= 42) {   // LinkInfo(Inq) REGISTER heartbeat
                gsRegTs = (int)be32(16);           // 32-bit Unix timestamp (payload[1:5])
                wanGps(32);                        // own WAN -> GPS (embedded LAN@38 == src)
                gsClass = "heartbeat_reg";
            } else if (packet_len == 0x1C) { gsClass = "ondemand_poll"; }
            else if (packet_len == 0x19) { gsClass = "ondemand_resp"; }
            else if (packet_len >= 0x40) { gsClass = "report"; }
            else if (packet_len == 17 || packet_len == 22 || packet_len == 23) {   // STATUS keep-alive
                if (data.size() > 16) { gsStatusSeq = data[15]; gsStatusFlags = data[16]; }
                gsClass = "status";
            } else { gsClass = "unicast"; }
        }
        {   // hop-clock ms from the 6-byte trailer (faithful port of _epoch_ms_from_body)
            size_t end = data.size();
            while (end > 0 && data[end - 1] == 0x04) end--;
            if (end >= 6) {
                const uint8_t* tr = &data[end - 6];
                if ((tr[3] & 0x0F) == 0) {
                    int ms = ((tr[0] << 8) | tr[1]) * 10 + (tr[3] >> 4);
                    if (ms >= 0 && ms < 168000) gsHopMs = ms;   // 168 s FHSS cycle
                }
            }
        }
    } else if ((packet_type == 0xD2 || packet_type == 0x52) && struct_ok) {
        gsClass = "link_p2p";                      // routed point-to-point (header_len=5, no link CRC)
        if (data.size() > 5) {
            gsD2Op = data[5];                      // D2 opcode at data[5]
            if (gsD2Op == 0x21 && data.size() >= 10) {   // neighbour/route advert -> real meter LAN
                gsSrc = hexn(6, 4); gsD2Advert = true;
                receivedMeterLanSrcID = u32id(gsSrc);
            }
        }
    } else if (block_mode) {                       // 0x81 long AES uplink (block-mode)
        gsClass = "long_report";
        // On-air ciphertext = AES-256-ECB(+CTS)( LZSS(payload) || CRC16 ). Tag the ENCRYPTED span
        // inside the reassembled block payload (gs_payload = blk_payload) so the downstream
        // GridStreamDecrypt block knows exactly what to AES-decrypt. The cleartext routing header
        // precedes the 80 E0 00 marker; the ciphertext starts right after it (field map, validated
        // in docs/Long_Packet_0x81_Findings.md). The routed variant uses a C0 50 .. header instead.
        gsEnc = true;
        for (size_t i = 0; i + 3 <= blk_payload.size(); i++) {
            if (blk_payload[i] == 0x80 && blk_payload[i + 1] == 0xE0 && blk_payload[i + 2] == 0x00) {
                gsCtOff = (int)(i + 3); break;
            }
        }
        if (gsCtOff >= 0) gsCtLen = (int)blk_payload.size() - gsCtOff;
        // End-to-end network routing header (validated: docs/WAN_LAN_Addressing.md §10 + the 0x81
        // field map). After the [81][ctrl] prefix, blk_payload holds the address block
        // [dstWAN:6][dstLAN:4][srcWAN:6][srcLAN:4] at offsets 2/8/12/18. Raw bytes, hex; a non-GPS
        // node uses the 0xFE.. WAN form, the universal head-end destination is 00 00 00 00 00 1F.
        if (blk_payload.size() >= 22) {
            auto bx = [&](size_t o, size_t n){ std::string r; for (size_t i=0;i<n;i++) r += char_to_hex(int(blk_payload[o+i])); return r; };
            gsDstWan = bx(2,6); gsDstLan = bx(8,4); gsSrcWan = bx(12,6); gsSrcLan = bx(18,4);
        }
    }
    double center_frequency = 0;
	if (pmt::dict_has_key(meta, pmt::intern("center_frequency"))) {
        center_frequency = pmt::to_double(pmt::dict_ref(meta, pmt::intern("center_frequency"), pmt::PMT_NIL));
    }
    int symbol_rate = 0;
    if (pmt::dict_has_key(meta, pmt::intern("symbol_rate"))) {
        symbol_rate = pmt::to_double(pmt::dict_ref(meta, pmt::intern("symbol_rate"), pmt::PMT_NIL));
    }
    std::time_t captured_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (pmt::dict_has_key(meta, pmt::intern("system_time"))) {
        captured_time = pmt::to_double(pmt::dict_ref(meta, pmt::intern("system_time"), pmt::PMT_NIL));
    }
    // 55/D5 pass on CRC-OK; CRC-NA types (D2 etc.) pass on structural validity instead
    // of being dropped. !d_crcEnable still passes everything through (CRC filter off).
    bool passes_validity = crc_applies ? (crc_state == 1) : struct_ok;
    if (((passes_validity) || !(d_crcEnable)) &&
        (((receivedMeterLanSrcID == d_meterLanSrcID) || (receivedMeterLanDstID == d_meterLanDstID)) ||
        ((d_meterLanDstID == 0) && (d_meterLanSrcID == 0))) &&
        ((packet_len == d_packetLengthFilter) || (d_packetLengthFilter == 0)) &&
        ((packet_type == d_packetTypeFilter) || (d_packetTypeFilter == 0)))
        {
            if (d_debugEnable) {
                std::cout << std::setfill('0') << std::hex << std::setw(2) << std::uppercase;
                if (!d_crcEnable && d_crcColor) {
                    if (crc_state == 1) {
                        std::cout << "\033[38;5;40m[CRC: OK] \033[m";   // Green: CRC match
                    } else if (crc_state == 2) {
                        std::cout << "\033[38;5;124m[CRC:BAD] \033[m";  // Red: CRC mismatch
                    } else {
                        std::cout << "\033[38;5;220m[CRC: NA] \033[m";  // Yellow: no link CRC (routed)
                    }
                } else if (!d_crcEnable && !d_crcColor) {
                    if (crc_state == 1) {
                        std::cout << "[CRC: OK] ";
                    } else if (crc_state == 2) {
                        std::cout << "[CRC:BAD] ";
                    } else {
                        std::cout << "[CRC: NA] ";
                    }
                }
                if (block_mode) {
                    std::cout << "[0x81 blocks " << std::dec << blk_ok << "/" << blk_count
                              << (blk_complete ? "" : " INCOMPLETE") << "] " << std::hex;
                }
                for (int i = 0; i < decoded_packet_size; i++)
                    {
                    std::cout << std::setw(2) << int(data[i]);
                    }
                if (d_baudrateEnable) {
                    std::cout << "\tBaudrate: " << std::dec << std::fixed << std::setprecision(0) << ((symbol_rate+25)/100)*100;
                }
                if (d_frequencyEnable) {
                    std::cout << "\tFreq: " << std::dec << std::fixed << std::setprecision(1) << floor(center_frequency/100000)/10;
                }
                if (d_epochEnable) {
                    std::cout << "\t" << time_in_HH_MM_SS_MMM();
                }
                if (d_timestampEnable) {
                    std::cout << "\t" << std::ctime(&captured_time);
                } else {
                    std::cout << "\n";
                }
            }
            // ---- PDU metadata (redesigned schema; old->new mapping at the extraction site) ----
            meta = pmt::dict_add(meta, pmt::mp("gs_ver"),   pmt::from_long(gsVer));
            meta = pmt::dict_add(meta, pmt::mp("gs_mtype"), pmt::from_long((long)data[2]));
            meta = pmt::dict_add(meta, pmt::mp("gs_ptype"), pmt::from_long((long)packet_type));
            meta = pmt::dict_add(meta, pmt::mp("gs_len"),   pmt::from_long((long)packet_len));
            meta = pmt::dict_add(meta, pmt::mp("gs_crc"),
                                 pmt::mp(std::string(crc_state == 1 ? "OK" : crc_state == 2 ? "BAD" : "NA")));
            meta = pmt::dict_add(meta, pmt::mp("gs_freq"), pmt::mp(double(floor(center_frequency/100000)/10)));
            if (crc_fixed_bit >= 0)
                meta = pmt::dict_add(meta, pmt::mp("gs_crc_fixed"), pmt::from_long(crc_fixed_bit));
            if (!gsSrc.empty())     meta = pmt::dict_add(meta, pmt::mp("gs_txlan"), pmt::mp(gsSrc));
            if (!gsDst.empty())     meta = pmt::dict_add(meta, pmt::mp("gs_rxlan"), pmt::mp(gsDst));
            if (!gsWan.empty())     meta = pmt::dict_add(meta, pmt::mp("gs_txwan"), pmt::mp(gsWan));
            if (!gsProto.empty())   meta = pmt::dict_add(meta, pmt::mp("gs_proto"), pmt::mp(gsProto));
            if (!gsFType.empty())   meta = pmt::dict_add(meta, pmt::mp("gs_ftype"), pmt::mp(gsFType));
            if (!gsClass.empty())   meta = pmt::dict_add(meta, pmt::mp("gs_class"), pmt::mp(gsClass));
            if (gsOpcode >= 0)      meta = pmt::dict_add(meta, pmt::mp("gs_opcode"), pmt::from_long(gsOpcode));
            if (gsFrameId >= 0)     meta = pmt::dict_add(meta, pmt::mp("gs_frameid"), pmt::from_long(gsFrameId));
            if (gsBcast)            meta = pmt::dict_add(meta, pmt::mp("gs_bcast"), pmt::PMT_T);
            if (gsHaveGps) {
                meta = pmt::dict_add(meta, pmt::mp("gs_lat"),   pmt::mp(gsLat));
                meta = pmt::dict_add(meta, pmt::mp("gs_lon"),   pmt::mp(gsLon));
                meta = pmt::dict_add(meta, pmt::mp("gs_color"), pmt::from_long(gsColor));
            }
            if (upTime)             meta = pmt::dict_add(meta, pmt::mp("gs_uptime"), pmt::from_long(upTime));
            if (gsRegTs)            meta = pmt::dict_add(meta, pmt::mp("gs_regts"), pmt::from_long(gsRegTs));
            if (gsStatusSeq >= 0)   meta = pmt::dict_add(meta, pmt::mp("gs_statusseq"), pmt::from_long(gsStatusSeq));
            if (gsStatusFlags >= 0) meta = pmt::dict_add(meta, pmt::mp("gs_statusflags"), pmt::from_long(gsStatusFlags));
            if (gsHopMs >= 0)       meta = pmt::dict_add(meta, pmt::mp("gs_hopms"), pmt::from_long(gsHopMs));
            if (gsD2Op >= 0)        meta = pmt::dict_add(meta, pmt::mp("gs_d2op"), pmt::from_long(gsD2Op));
            if (gsD2Advert)         meta = pmt::dict_add(meta, pmt::mp("gs_d2advert"), pmt::PMT_T);
            if (block_mode) {
                meta = pmt::dict_add(meta, pmt::mp("gs_blockmode"), pmt::PMT_T);
                meta = pmt::dict_add(meta, pmt::mp("gs_blocks"),    pmt::from_long(blk_count));
                meta = pmt::dict_add(meta, pmt::mp("gs_blocks_ok"), pmt::from_long(blk_ok));
                meta = pmt::dict_add(meta, pmt::mp("gs_complete"),  pmt::from_bool(blk_complete));
                meta = pmt::dict_add(meta, pmt::mp("gs_block_crc"), pmt::init_u8vector(blk_results.size(), blk_results));
                meta = pmt::dict_add(meta, pmt::mp("gs_payload"),   pmt::init_u8vector(blk_payload.size(), blk_payload));
                // encrypted-span tags for the downstream GridStreamDecrypt block
                if (gsEnc)       meta = pmt::dict_add(meta, pmt::mp("gs_enc"),    pmt::mp(std::string("encrypted")));
                if (gsCtOff >= 0) meta = pmt::dict_add(meta, pmt::mp("gs_ct_off"), pmt::from_long(gsCtOff));
                if (gsCtLen >= 0) meta = pmt::dict_add(meta, pmt::mp("gs_ct_len"), pmt::from_long(gsCtLen));
                if (!gsDstWan.empty()) meta = pmt::dict_add(meta, pmt::mp("gs_dstwan"), pmt::mp(gsDstWan));
                if (!gsDstLan.empty()) meta = pmt::dict_add(meta, pmt::mp("gs_dstlan"), pmt::mp(gsDstLan));
                if (!gsSrcWan.empty()) meta = pmt::dict_add(meta, pmt::mp("gs_srcwan"), pmt::mp(gsSrcWan));
                if (!gsSrcLan.empty()) meta = pmt::dict_add(meta, pmt::mp("gs_srclan"), pmt::mp(gsSrcLan));
            }
            message_port_pub(PMTCONSTSTR__PDU_OUT,(pmt::cons(meta, pmt::init_u8vector(data.size(), data))));
            return;
        }
        else {
            return;
        }
}

} /* namespace smart_meters */
} /* namespace gr */
