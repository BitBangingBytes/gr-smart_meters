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
		uint16_t d_crcInitialValue;
		uint32_t d_meterMonitorID;
		uint8_t d_packetTypeFilter;
		uint16_t d_packetLengthFilter;
        uint16_t crc16(uint16_t crc, const std::vector<uint8_t> &data, size_t size);

     //protected:
     // int calculate_output_stream_length(const gr_vector_int &ninput_items);

     public:
      GridStream_impl(bool crcEnable, uint16_t crcInitialValue, uint32_t meterMonitorID, uint8_t packetTypeFilter, uint16_t packetLengthFilter);
      ~GridStream_impl();

      // Where all the action really happens
      void pdu_handler(pmt::pmt_t pdu);
    };

  } // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_SMART_METERS_GRIDSTREAM_IMPL_H */

