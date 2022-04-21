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
     */
    static sptr make(bool     crcEnable,
                     bool     debugEnable,
                     bool     timestampEnable,
                     bool     epochEnable,
                     bool     frequencyEnable,
                     bool     baudrateEnable,
                     uint16_t crcInitialValue,
                     uint32_t meterLanSrcID,
                     uint32_t meterLanDstID,
                     uint8_t  packetTypeFilter,
                     uint16_t packetLengthFilter);
};

} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAM_H */
