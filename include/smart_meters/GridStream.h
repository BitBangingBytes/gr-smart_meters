/* -*- c++ -*- */
/*
 * Copyright 2021 Hash.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_SMART_METERS_GRIDSTREAM_H
#define INCLUDED_SMART_METERS_GRIDSTREAM_H

#include <gnuradio/block.h>
#include <smart_meters/api.h>

namespace gr {
namespace smart_meters {

/*!
 * \brief <+description of block+>
 * \ingroup smart_meters
 *
 */
class SMART_METERS_API GridStream : virtual public gr::block
{
public:
    typedef std::shared_ptr<GridStream> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of smart_meters::GridStream.
     *
     * To avoid accidental use of raw pointers, smart_meters::GridStream's
     * constructor is in a private implementation
     * class. smart_meters::GridStream::make is the public interface for
     * creating new instances.
     *
     * \param crcCorrectBits  0 = off (default). 1 = a 0x55/0xD5 frame whose CRC fails is
     *        repaired when exactly one flipped bit in its body+CRC makes the CRC match. CRC-16/CCITT
     *        has Hamming distance 4 at GridStream lengths, so a unique one-bit fix still leaves
     *        two-bit errors detected. Repaired frames carry meta gs_crc_fixed = the bit index
     *        (MSB-first, counted from the start of the PDU) and gs_crc "OK".
     */
    static sptr make(bool crcEnable,
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
                     uint8_t  crcCorrectBits = 0);
};

} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAM_H */
