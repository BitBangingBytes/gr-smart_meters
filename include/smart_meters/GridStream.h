/* -*- c++ -*- */
/*
 * Copyright 2021 Hash.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_SMART_METERS_GRIDSTREAM_H
#define INCLUDED_SMART_METERS_GRIDSTREAM_H

#include <smart_meters/api.h>
#include <gnuradio/block.h>

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
      typedef boost::shared_ptr<GridStream> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of smart_meters::GridStream.
       *
       * To avoid accidental use of raw pointers, smart_meters::GridStream's
       * constructor is in a private implementation
       * class. smart_meters::GridStream::make is the public interface for
       * creating new instances.
       */
      static sptr make(bool crcEnable, uint16_t crcInitialValue, uint32_t meterMonitorID, uint8_t packetTypeFilter, uint16_t packetLengthFilter);
    };

  } // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAM_H */

