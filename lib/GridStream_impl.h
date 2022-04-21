/* -*- c++ -*- */
/*
 * Copyright 2021 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_SMART_METERS_GRIDSTREAM_IMPL_H
#define INCLUDED_SMART_METERS_GRIDSTREAM_IMPL_H

#include <smart_meters/GridStream.h>
#include <smart_meters/constants.h>

namespace gr {
namespace smart_meters {

class GridStream_impl : public GridStream
{
private:
    bool d_crcEnable;
    bool d_debugEnable;
    bool d_timestampEnable;
    bool d_epochEnable;
    bool d_frequencyEnable;
    bool d_baudrateEnable;
    uint16_t d_crcInitialValue;
    uint32_t d_meterLanSrcID;
    uint32_t d_meterLanDstID;
    uint8_t  d_packetTypeFilter;
    uint16_t d_packetLengthFilter;
    uint16_t crc16(uint16_t crc, const std::vector<uint8_t>& data, size_t size);

    /*!
     * \brief Message handler for input messages
     *
     * \param msg Dict PMT or PDU message passed from the scheduler's message handling.
     */
    void pdu_handler(pmt::pmt_t pdu);

public:
    GridStream_impl(bool crcEnable,
					bool debugEnable,
					bool timestampEnable,
                    bool epochEnable,
					bool frequencyEnable,
					bool baudrateEnable,
                    uint16_t crcInitialValue,
                    uint32_t meterLanSrcID,
                    uint32_t meterLanDstID,
                    uint8_t  packetTypeFilter,
                    uint16_t packetLengthFilter);
    ~GridStream_impl() override;
};

} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAM_IMPL_H */
