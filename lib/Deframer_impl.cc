/* -*- c++ -*- */
/*
 * Copyright 2021, 2022 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Deframer_impl.h"
#include <gnuradio/io_signature.h>
#include <iomanip>

namespace gr {
namespace smart_meters {

Deframer::sptr Deframer::make(uint16_t min_length, uint16_t max_length, bool debug)
{
    return gnuradio::make_block_sptr<Deframer_impl>(min_length, max_length, debug);
}

/*
 * The private constructor
 */
Deframer_impl::Deframer_impl(uint16_t min_length, uint16_t max_length, bool debug)
    : gr::block(
        "Deframer", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
     d_min_length(min_length),
     d_max_length(max_length),
     d_debug(debug)   
{
    message_port_register_in(PMTCONSTSTR__PDU_IN);
    set_msg_handler(PMTCONSTSTR__PDU_IN,
                    [this](pmt::pmt_t pdu) { this->pdu_handler(pdu); });
    message_port_register_out(PMTCONSTSTR__PDU_OUT);

}

/*
 * Our virtual destructor.
 */
Deframer_impl::~Deframer_impl() {}

uint8_t
Deframer_impl::parse_byte(const std::vector<uint8_t>& data, int offset) 
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        // MSB FIRST PROCESSING
        byte >>= 1;
        offset += 1;
        if (data[offset])
            byte |= 0x80;
    }
    return byte;
}

int
Deframer_impl::check_gridstream_version(const std::vector<uint8_t>& data) 
{
    const uint32_t version4 = 0xFFA0;
    const uint32_t version5 = 0xFFF0;
    uint32_t start_of_frame = 0;
    int offset = 0;
    for (int i = 0; i < 20; i++) {
        // MSB FIRST PROCESSING
        start_of_frame >>= 1;
        if (data[offset])
            start_of_frame |= 0x8000;
        offset += 1;
    }
    if (start_of_frame == version5) {
        return 5;
    } else if (start_of_frame == version4) {
        return 4;
    } else {
        return 0;
    }
}

void Deframer_impl::pdu_handler(pmt::pmt_t pdu)
{
    pmt::pmt_t meta = {};
    pmt::pmt_t v_data = {};
    meta = pmt::car(pdu);
    v_data = pmt::cdr(pdu);
	
    // make sure PDU data is formed properly
    if (!(pmt::is_pdu(pdu))) {
        GR_LOG_WARN(d_logger, "received unexpected PMT (non-pdu)");
        return;
    }
    
    // Add some buffer to the data so we don't move beyond the end checking bit offset's
    size_t vlen = pmt::length(pmt::cdr(pdu));
    std::vector<uint8_t> input_data = pmt::u8vector_elements(v_data);
    input_data.resize(vlen+10);
    const std::vector<uint8_t> data = input_data;

    const int header = 2;
    if ( ((vlen/10) < header) || ((vlen/10) < d_min_length) || ((vlen/10) > d_max_length) ) {
        return;
    }
    std::vector<uint8_t> out;
    out.reserve(data.size());
    fill(out.begin(), out.end(), 0);

    int offset = 0;
    int bytesToProcess = vlen/10;
    int bytesProcessed = 0;

    int gridstream_version = check_gridstream_version(data);
    meta = pmt::dict_add(meta, pmt::mp("GridStream_Version"), pmt::mp(gridstream_version));
    if (gridstream_version == 4) {
        out.push_back(0x00);
        out.push_back(0xFF);
        offset += 20;
        bytesProcessed += 2;
        bytesToProcess -= 2;
    }
    if (gridstream_version == 5) {
        out.push_back(0x80);
        out.push_back(0xFF);
        offset += 20;
        bytesProcessed += 2;
        bytesToProcess -= 2;
    }

    uint8_t byte = 0;    
    int framingErrorCounter = 0;
    for (int i = 0; i < bytesToProcess; i++) {
        bool normalStartStopBitLocation = (!data[offset] && data[offset+9]);
        bool shiftedStartStopBitLocation = (!data[offset+1] && data[offset+10]);
        if (normalStartStopBitLocation) {
            framingErrorCounter = 0;
            byte = parse_byte(data, offset);
            out.push_back(byte);
            bytesProcessed++;
            offset += 10;
        } 
        else if (shiftedStartStopBitLocation) {
            framingErrorCounter += 1;
            offset += 1;
            if (gridstream_version == 5) {
                byte = parse_byte(data, offset);
                out.push_back(byte);
                bytesProcessed++;
                offset += 10;
                if (data[offset] && data[offset+9]) {
                    byte = parse_byte(data, offset);
                    out.push_back(byte);
                    bytesProcessed++;
                    offset += 10;
                    i++;
                }
                if (data[offset] && !data[offset+1]) {
                    offset += 1;
                }
            } else {
                byte = parse_byte(data, offset);
                out.push_back(byte);
                bytesProcessed++;
                offset += 10;
            }
        }
        if (framingErrorCounter > 1) {
            break;
        }
    }
    out.resize(bytesProcessed);
    
    if (d_debug) {
        std::cout << "GridStream Version: " << gridstream_version << "\n";
        std::cout << std::setfill('0') << std::hex << std::setw(2) << std::uppercase;
        for (int i = 0; i < out.size(); i++) {
            std::cout << std::setw(2) << int(out[i]);
        }
        std::cout << "\n";
    }
    if (out.size() > 0) {
        message_port_pub(PMTCONSTSTR__PDU_OUT,(pmt::cons(meta, pmt::init_u8vector(out.size(), out))));
    }
    return;
}

} /* namespace smart_meters */
} /* namespace gr */
