/* -*- c++ -*- */
/*
 * Copyright 2021, 2022 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_SMART_METERS_DEFRAMER_IMPL_H
#define INCLUDED_SMART_METERS_DEFRAMER_IMPL_H

#include <smart_meters/Deframer.h>
#include <smart_meters/constants.h>

namespace gr {
namespace smart_meters {

class Deframer_impl : public Deframer
{
private:
    uint16_t d_min_length;
    uint16_t d_max_length;
    bool d_debug;
    bool d_multi_sync;
    bool d_tolerant;
    /*!
     * \brief Message handler for input messages
     *
     * \param pdu Dict PMT or PDU message passed from the scheduler's message handling.
     */
    void pdu_handler(pmt::pmt_t pdu);
    int process_byte(const std::vector<uint8_t>& data, std::vector<uint8_t>& out, int& offset);
    int process_gridstream_header(const std::vector<uint8_t>& data, std::vector<uint8_t>& out);
    bool verify_v5_special_pattern(const std::vector<uint8_t>& data, int offset);
    uint32_t sync_value_at(const std::vector<uint8_t>& data, int offset);
    int decode_burst(const std::vector<uint8_t>& data, std::vector<uint8_t>& out, int& offset,
                     int end_bits, int version);


public:
    Deframer_impl(uint16_t min_length, uint16_t max_length, bool debug, bool multi_sync,
                  bool tolerant_framing);
    ~Deframer_impl();

};

} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_DEFRAMER_IMPL_H */
