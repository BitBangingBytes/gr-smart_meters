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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "GridStream_impl.h"
#include <chrono>
#include <ctime>  
#include <iostream>
#include <fstream>
#include <string>  
#include <sstream> 

namespace gr {
  namespace smart_meters {

    GridStream::sptr
    GridStream::make(bool crcEnable, uint16_t crcInitialValue, uint32_t meterMonitorID, uint8_t packetTypeFilter, uint16_t packetLengthFilter)
    {
      return gnuradio::get_initial_sptr
        (new GridStream_impl(crcEnable, crcInitialValue, meterMonitorID, packetTypeFilter, packetLengthFilter));
    }


    /*
     * The private constructor
     */
    GridStream_impl::GridStream_impl(bool crcEnable, uint16_t crcInitialValue, uint32_t meterMonitorID, uint8_t packetTypeFilter, uint16_t packetLengthFilter)
      : gr::block("GridStream",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0)),
              d_crcEnable(crcEnable),
              d_crcInitialValue(crcInitialValue),
              d_meterMonitorID(meterMonitorID),
              d_packetTypeFilter(packetTypeFilter),
              d_packetLengthFilter(packetLengthFilter)
    {
    message_port_register_in(PMTCONSTSTR__PDU_IN);
    set_msg_handler(PMTCONSTSTR__PDU_IN,
                    boost::bind(&GridStream_impl::general_work, this, _1));
    message_port_register_out(PMTCONSTSTR__PDU_OUT);    	    
	}

    /*
     * Our virtual destructor.
     */
    GridStream_impl::~GridStream_impl()
    {
    }

	template< typename T >
	std::string int_to_hex( T i )
	{
	  std::stringstream stream;
	  stream << std::setfill ('0') << std::setw(sizeof(T)*2) 
			 << std::hex << std::uppercase << i;
	  return stream.str();
	}

	template< typename T >
	std::string char_to_hex( T i )
	{
	  std::stringstream stream;
	  stream << std::setfill ('0') << std::setw(2) 
			 << std::hex << std::uppercase << i;
	  return stream.str();
	}
	
    uint16_t     
	GridStream_impl::crc16(uint16_t crc, const std::vector<uint8_t> &data, size_t size) {
		// Some known CRC's below
		// uint16_t crc = 0x45F8;	// (CoServ CRC)
		// uint16_t crc = 0x5FD6;	// (Oncor CRC)
		// uint16_t crc = 0x62C1;	// (Hydro-Quebec CRC)
		// Hard coded Poly 0x1021
	    uint16_t i = 6; 			// Skip over header/packet length [00,FF,2A,55,xx,xx]
	    while (size--) {       
			crc ^= data[i] << 8;
			i++;
			for (unsigned k = 0; k < 8; k++)
				crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;     
	    }     
		return crc; 
    } 

    void
    GridStream_impl::pdu_handler (pmt::pmt_t pdu)
    {
		pmt::pmt_t meta = pmt::car(pdu);
		pmt::pmt_t v_data = pmt::cdr(pdu);

		// make sure PDU data is formed properly
		if (!(pmt::is_pair(pdu))) {
			GR_LOG_WARN(d_logger, "received unexpected PMT (non-pair)");
			return;
		}
		if (!pmt::is_uniform_vector(pmt::cdr(pdu))) {
			GR_LOG_WARN(d_logger, "received unexpected PMT (CDR not uniform vector)");
			return;
		}
		//
		
		size_t vlen = pmt::length(pmt::cdr(pdu));
		const std::vector<uint8_t> data = pmt::u8vector_elements(v_data);

		// output data from block, 10 bytes input per 1 byte output, +5 buffer
		std::vector<uint8_t> out;
		out.reserve((data.size()/10)+5);			
		// Packet not large enough, probably noise
		if (data.size() < 9)
			return;				
		// Read header and packet length
		uint8_t byte = 0;
		long offset = 1;
		for (int ii=0; ii<4+2; ii++) {
			for (int jj = 0; jj < 8; jj++) {
				// START MSB FIRST PROCESSING
				byte >>= 1;
				if (data[jj+offset])
					byte |= 0x80;
				if ((jj % 8) == 7) {
					out.push_back(byte);
					byte = 0;
					}	
			}
			offset += 10;
		}
		// Packet decoded 00,FF,2A,packet_type(xx),packet_len(xxxx)
	    int packet_type = out[3];
	    int packet_len = out[5] | out[4] << 8;
	    
	    // Loop to decode data based on packet_len
	    if (data.size() > packet_len) {
			for (int ii=0; ii < packet_len; ii++) {
				uint8_t byte = 0;
				for (int jj = 0; jj < 8; jj++) {
					// START MSB FIRST PROCESSING
					byte >>= 1;
					if (data[jj+offset])
						byte |= 0x80;
					if ((jj % 8) == 7) {
						out.push_back(byte);
						byte = 0;
					}
				}
				offset += 10;	
			}
		}
		else {
			return;
		}
		int meterID {0};
		int meterID2 {0};
		int upTime {0};
		
		if (packet_type == 0x55 && packet_len == 0x0023)
		{
			meterID = out[27+2] | out[26+2] << 8 | out[25+2] << 16 | out[24+2] << 24;
			upTime = out[21+2] | out[20+2] << 8 | out[19+2] << 16 | out[18+2] << 24;
		} else if (packet_type == 0xD5)
		{
			meterID2 = out[8+2] | out[7+2] << 8 | out[6+2] << 16 | out[5+2] << 24;
			meterID = out[12+2] | out[11+2] << 8 | out[10+2] << 16 | out[9+2] << 24;
		} else
		{
			return;
		}

		int receivedCRC = out[packet_len+5] | out[packet_len+4] << 8;
		uint16_t calculatedCRC = GridStream_impl::crc16(d_crcInitialValue, out, out.size()-8); //Strip off header/len (6) and crc (2)

	if( ((receivedCRC == calculatedCRC) || !(d_crcEnable)) && 
	    ((meterID == d_meterMonitorID) || (meterID2 == d_meterMonitorID) || (d_meterMonitorID == 0)) &&
	    ((packet_len == d_packetLengthFilter) || (d_packetLengthFilter == 0)) &&
	    ((packet_type == d_packetTypeFilter) || (d_packetTypeFilter == 0)) ) 
	    {
			std::cout << std::setfill('0') << std::hex << std::setw(2) << std::uppercase;
			for (int i = 0; i < packet_len+4+2; i++) // +4 to capture leading bytes and -2 to strip off CRC -1 to strip off end of packet
			{
				std::cout << std::setw(2) << int(out[i]);
			}
			std::cout << "\n";

			message_port_pub(PMTCONSTSTR__PDU_OUT,(pmt::cons(meta, pmt::init_u8vector(out.size(), out))));
			return;
		}
	else {
		return;
		}
    }

  } /* namespace smart_meters */
} /* namespace gr */

