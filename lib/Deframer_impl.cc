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

int
Deframer_impl::process_byte(const std::vector<uint8_t>& data, std::vector<uint8_t>& out, int& offset) 
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        // MSB FIRST PROCESSING
        byte >>= 1;
        offset += 1;
        if (data[offset])
            byte |= 0x80;
    }
    out.push_back(byte);
    offset += 2;
    return 1;
}

int
Deframer_impl::check_gridstream_version(const std::vector<uint8_t>& data, std::vector<uint8_t>& out) 
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
        out.push_back(0x80);
        out.push_back(0xFF);         
        return 5;
    } else if (start_of_frame == version4) {
        out.push_back(0x00);
        out.push_back(0xFF);
        return 4;
    } else {       
        return 0;
    }
}

void Deframer_impl::pdu_handler(pmt::pmt_t pdu)
{
    pmt::pmt_t meta = {};
    pmt::pmt_t vector_data = {};
    meta = pmt::car(pdu);
    vector_data = pmt::cdr(pdu);
	
    // make sure PDU data is formed properly
    if (!(pmt::is_pdu(pdu))) {
        GR_LOG_WARN(d_logger, "received unexpected PMT (non-pdu)");
        return;
    }
    
    // Add some buffer to the data so we don't move beyond the end checking bit offset's
    size_t v_data_len = pmt::length(vector_data);
    std::vector<uint8_t> input_data = pmt::u8vector_elements(vector_data);
    input_data.resize(v_data_len + 10);
    const std::vector<uint8_t> data = input_data;

    const int header_minimum = 2;
    if ( ((v_data_len/10) < header_minimum) || ((v_data_len/10) < d_min_length) || ((v_data_len/10) > d_max_length) ) {
        return;
    }
    std::vector<uint8_t> out;
    out.reserve(data.size());
    fill(out.begin(), out.end(), 0);

    int offset = 0;
    int bytesToProcess = v_data_len / 10;
    int bytesProcessed = 0;

    int gridstream_version = check_gridstream_version(data, out);
    meta = pmt::dict_add(meta, pmt::mp("GridStream_Version"), pmt::mp(gridstream_version));
    if (gridstream_version != 0) {
        offset += 20;
        bytesProcessed += 2;
    }

    uint8_t byte = 0;    
    int num_frame_errors = 0;
    bytesToProcess -= bytesProcessed;
    // Process Frames
    for (int i = 0; i < bytesToProcess; i++) {
        bool current_frame = (!data[offset] && data[offset+9]);
        bool new_frame = (!data[offset+1] && data[offset+10]);
        if (current_frame) {
            num_frame_errors = 0;
            bytesProcessed += process_byte(data, out, offset);
        } 
        else if (new_frame) {
            num_frame_errors += 1;
            offset += 1;
            if (gridstream_version == 5) {
                bytesProcessed += process_byte(data, out, offset);
                if (data[offset] && data[offset+9] && data[offset+10] && !data[offset+11]) {
                    bytesProcessed += process_byte(data, out, offset);
                    offset += 1;
                    i++;
                }
            } else {
                bytesProcessed += process_byte(data, out, offset);
            }
        }
        if (num_frame_errors > 1) {
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
