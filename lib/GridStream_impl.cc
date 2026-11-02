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
					uint16_t packetLengthFilter)
{
    return gnuradio::make_block_sptr<GridStream_impl>(
        crcEnable, debugEnable, crcColor, timestampEnable, epochEnable, frequencyEnable, baudrateEnable,
        crcInitialValue, meterLanSrcID, meterLanDstID, packetTypeFilter, packetLengthFilter);
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
					uint16_t packetLengthFilter)
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
      d_packetLengthFilter(packetLengthFilter)
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
    const std::vector<uint8_t> data = pmt::u8vector_elements(vector_data);

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
        // Block boundaries come from the FIRMWARE LAYOUT, not from searching for "00 FF": with no
        // inter-burst gap the Deframer decodes straight through the re-sync, and a boundary is then
        // bit-identical to a 00 FF pair inside ciphertext. After a non-last block (hdr, 80 data, lo)
        // the next burst starts right away, or after one 04 tail byte (TX state 0x18). The Deframer's
        // GridStream_Bursts (multi_sync) marks bursts it re-synced on after a real gap; those are
        // used only to re-align when the layout position does not hold a 00|80 FF.
        std::vector<uint32_t> resync;
        if (pmt::dict_has_key(meta, pmt::intern("GridStream_Bursts"))) {
            resync = pmt::u32vector_elements(
                pmt::dict_ref(meta, pmt::intern("GridStream_Bursts"), pmt::PMT_NIL));
        }
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
                size_t r = 0;                                // re-align on a real gap re-sync
                for (uint32_t x : resync)
                    if (x > st + 3) { r = x; break; }
                if (r == 0) break;
                st = r;
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
                dlen = BLK;
                if (s0 + 3 + BLK < e0)
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

    uint32_t receivedMeterLanSrcID{ 0xFFFFFFFF };
    uint32_t receivedMeterLanDstID{ 0xFFFFFFFF };
    int upTime{ 0 };
	std::string GridStreamMeterSrcID{ "" };
	std::string GridStreamMeterSrcWanID{ "" };
	std::string GridStreamMeterDstID{ "" };

    if (crc_state == 1) {   // CRC-OK 0x55/0xD5 only
        if ( (packet_type == 0x55) && (packet_len == 0x0023) ) {

            receivedMeterLanSrcID = data[27 + 2] | data[26 + 2] << 8 |
                                    data[25 + 2] << 16 | data[24 + 2] << 24;

            GridStreamMeterSrcID = (char_to_hex(int(data[26]))+char_to_hex(int(data[27]))+
                                    char_to_hex(int(data[28]))+char_to_hex(int(data[29])));

            GridStreamMeterSrcWanID = (char_to_hex(int(data[13]))+char_to_hex(int(data[14]))+
                                       char_to_hex(int(data[15]))+char_to_hex(int(data[16]))+
                                       char_to_hex(int(data[17]))+char_to_hex(int(data[18])));
            GridStreamMeterDstID = { "" };
            upTime = data[21 + 2] | data[20 + 2] << 8 | data[19 + 2] << 16 | data[18 + 2] << 24;
        }
        else if (packet_type == 0xD5) {
            receivedMeterLanSrcID = data[14] | data[13] << 8 | data[12] << 16 | data[11] << 24;
            receivedMeterLanDstID = data[10] | data[9] << 8 | data[8] << 16 | data[7] << 24;
            GridStreamMeterSrcID = (char_to_hex(int(data[11]))+char_to_hex(int(data[12]))+
                                    char_to_hex(int(data[13]))+char_to_hex(int(data[14])));
            GridStreamMeterDstID = (char_to_hex(int(data[7]))+char_to_hex(int(data[8]))+
                                    char_to_hex(int(data[9]))+char_to_hex(int(data[10])));
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
            meta = pmt::dict_add(meta, pmt::mp("Gridstream_LanSrcID"), pmt::mp(GridStreamMeterSrcID));
            meta = pmt::dict_add(meta, pmt::mp("Gridstream_WanSrcID"), pmt::mp(GridStreamMeterSrcWanID));
            meta = pmt::dict_add(meta, pmt::mp("Gridstream_LanDstID"), pmt::mp(GridStreamMeterDstID));
            meta = pmt::dict_add(meta, pmt::mp("Gridstream_Uptime"), pmt::mp(upTime));
            meta = pmt::dict_add(meta, pmt::mp("Gridstream_Freq"), pmt::mp(double(floor(center_frequency/100000)/10)));
            meta = pmt::dict_add(meta, pmt::mp("Gridstream_CRCState"),
                                 pmt::mp(std::string(crc_state == 1 ? "OK" : crc_state == 2 ? "BAD" : "NA")));

            if (block_mode) {
                meta = pmt::dict_add(meta, pmt::mp("Gridstream_BlockMode"), pmt::PMT_T);
                meta = pmt::dict_add(meta, pmt::mp("Gridstream_Blocks"), pmt::mp(blk_count));
                meta = pmt::dict_add(meta, pmt::mp("Gridstream_BlocksOK"), pmt::mp(blk_ok));
                meta = pmt::dict_add(meta, pmt::mp("Gridstream_Complete"), pmt::from_bool(blk_complete));
                meta = pmt::dict_add(meta, pmt::mp("Gridstream_BlockCRC"),
                                     pmt::init_u8vector(blk_results.size(), blk_results));
                meta = pmt::dict_add(meta, pmt::mp("Gridstream_Payload"),
                                     pmt::init_u8vector(blk_payload.size(), blk_payload));
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
