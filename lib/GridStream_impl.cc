/* -*- c++ -*- */
/*
 * Copyright 2021 Hash.
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
#include <sstream>
#include <string>

namespace gr {
namespace smart_meters {

GridStream::sptr GridStream::make(	bool crcEnable,
									bool debugEnable,
									bool timestampEnable,
									bool frequencyEnable,
									bool baudrateEnable,
									uint16_t crcInitialValue,
									uint32_t meterLanSrcID,
									uint32_t meterLanDstID,
									uint8_t  packetTypeFilter,
									uint16_t packetLengthFilter)
{
    return gnuradio::make_block_sptr<GridStream_impl>(
        crcEnable, debugEnable, timestampEnable, frequencyEnable, baudrateEnable, 
        crcInitialValue, meterLanSrcID, meterLanDstID, packetTypeFilter, packetLengthFilter);
}


/*
 * The private constructor
 */
GridStream_impl::GridStream_impl(bool crcEnable,
								 bool debugEnable,
								 bool timestampEnable,
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
      d_timestampEnable(timestampEnable),
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

uint16_t
GridStream_impl::crc16(uint16_t crc, const std::vector<uint8_t>& data, size_t size)
{
    // Some known CRC Init's below
    // uint16_t crc = 0x45F8;	// (CoServ CRC)
    // uint16_t crc = 0x5FD6;	// (Oncor CRC)
    // uint16_t crc = 0x62C1;	// (Hydro-Quebec CRC)
    // Hard coded Poly 0x1021
    uint16_t i = 6; // Skip over header/packet length [00,FF,2A,55,xx,xx]
    while (size--) {
        crc ^= data[i] << 8;
        i++;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

void GridStream_impl::pdu_handler(pmt::pmt_t pdu)
{
    pmt::pmt_t meta = pmt::car(pdu);
    pmt::pmt_t v_data = pmt::cdr(pdu);

    // make sure PDU data is formed properly
    if (!(pmt::is_pdu(pdu))) {
        GR_LOG_WARN(d_logger, "received unexpected PMT (non-pdu)");
        return;
    }
    size_t vlen = pmt::length(pmt::cdr(pdu));
    const std::vector<uint8_t> data = pmt::u8vector_elements(v_data);

    // Packet not large enough, probably noise
    if (data.size() < 120)
        return;

    // output data from block, 10 bytes input per 1 byte output
    std::vector<uint8_t> out;
    out.reserve(data.size() / 10);

    // Read header and packet length
    uint8_t byte = 0;
    long offset = 1;
    for (int ii = 0; ii < 4 + 2; ii++) {
        for (int jj = 0; jj < 8; jj++) {
            // START MSB FIRST PROCESSING
            byte >>= 1;
            if (data[jj + offset])
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
    std::cout << "Capacity: " << out.capacity() << " ";  //Debug
    if (out.capacity() >= (packet_len+6)) {
        for (int ii = 0; ii < packet_len; ii++) {
            uint8_t byte = 0;
            for (int jj = 0; jj < 8; jj++) {
                // START MSB FIRST PROCESSING
                byte >>= 1;
                if (data[jj + offset])
                    byte |= 0x80;
                if ((jj % 8) == 7) {
                    out.push_back(byte);
                    byte = 0;
                }
            }
            offset += 10;
        }
    } else {
        return;
    }
    int receivedCRC = out[packet_len + 5] | out[packet_len + 4] << 8;
    uint16_t calculatedCRC = GridStream_impl::crc16(d_crcInitialValue, out, out.size() - 8); // Strip off header/len (6) and crc (2)
	if (receivedCRC != calculatedCRC) {
		std::cout << "Bad CRC, Received: " << std::hex << std::setw(2) << std::uppercase << receivedCRC << " Calculated: " << calculatedCRC << " ";  //Debug
	}

    int receivedMeterLanSrcID{ 0 };
    int receivedMeterLanDstID{ 0 };
    int upTime{ 0 };
	std::string GridStreamMeterSrcID{ "" };
	std::string GridStreamMeterDstID{ "" };
    if (packet_type == 0x55 && packet_len == 0x0023) {
        receivedMeterLanSrcID = out[27 + 2] | 
								out[26 + 2] << 8 | 
								out[25 + 2] << 16 | 
								out[24 + 2] << 24;
        GridStreamMeterSrcID = (char_to_hex(int(out[26]))+
								char_to_hex(int(out[27]))+
								char_to_hex(int(out[28]))+
								char_to_hex(int(out[29])));
        GridStreamMeterDstID = "";
		upTime = out[21 + 2] | 
				 out[20 + 2] << 8 | 
				 out[19 + 2] << 16 | 
				 out[18 + 2] << 24;
    } else if (packet_type == 0xD5) {
        receivedMeterLanSrcID = out[14] | 
								out[13] << 8 | 
								out[12] << 16 | 
								out[11] << 24;
        receivedMeterLanDstID = out[10] | 
								out[9] << 8 | 
								out[8] << 16 | 
								out[7] << 24;
        GridStreamMeterSrcID = (char_to_hex(int(out[11]))+
								char_to_hex(int(out[12]))+
								char_to_hex(int(out[13]))+
								char_to_hex(int(out[14])));    
        GridStreamMeterDstID = (char_to_hex(int(out[7]))+
								char_to_hex(int(out[8]))+
								char_to_hex(int(out[9]))+
								char_to_hex(int(out[10])));            
    }

	double center_frequency = pmt::to_double(pmt::dict_ref(meta, pmt::intern("center_frequency"), pmt::PMT_NIL));
	int symbol_rate = pmt::to_double(pmt::dict_ref(meta, pmt::intern("symbol_rate"), pmt::PMT_NIL));

    if (((receivedCRC == calculatedCRC) || !(d_crcEnable)) &&
        ((receivedMeterLanSrcID == d_meterLanSrcID) || (d_meterLanSrcID == 0)) &&
        ((receivedMeterLanDstID == d_meterLanDstID) || (d_meterLanDstID == 0)) &&
        ((packet_len == d_packetLengthFilter) || (d_packetLengthFilter == 0)) &&
        ((packet_type == d_packetTypeFilter) || (d_packetTypeFilter == 0))) 
        {
			if (d_debugEnable) {
				std::cout << std::setfill('0') << std::hex << std::setw(2) << std::uppercase;
				for (int i = 0; i < packet_len + 6; i++) // +6 to include 00FF2Axxyyzz header  
					{
					std::cout << std::setw(2) << int(out[i]);
					}
				if (d_baudrateEnable) {
					std::cout << "\tBaudrate: " << std::dec << std::fixed << std::setprecision(0) << ((symbol_rate+25)/100)*100;			
				}
				if (d_frequencyEnable) {
					std::cout << "\tFreq: " << std::dec << std::fixed << std::setprecision(1) << floor(center_frequency/100000)/10;
				}
				if (d_timestampEnable) {
					std::time_t result = std::time(nullptr);
					std::cout << "\t" << std::ctime(&result);
				} else {
					std::cout << "\n";
				}
			}
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_LanSrcID"), pmt::mp(GridStreamMeterSrcID));
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_LanDstID"), pmt::mp(GridStreamMeterDstID));		
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_Uptime"), pmt::mp(upTime));
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_Freq"), pmt::mp(double(floor(center_frequency/100000)/10)));
			
			message_port_pub(PMTCONSTSTR__PDU_OUT,(pmt::cons(meta, pmt::init_u8vector(out.size(), out))));
			return;
		} 
		else {
			return;
		}
}

} /* namespace smart_meters */
} /* namespace gr */
