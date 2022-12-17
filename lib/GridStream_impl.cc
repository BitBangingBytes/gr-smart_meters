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
#include <iomanip>
#include <sstream>
#include <string>

namespace gr {
namespace smart_meters {

GridStream::sptr GridStream::make(	bool crcEnable,
									bool debugEnable,
									bool timestampEnable,
                                    bool epochEnable,
									bool frequencyEnable,
									bool baudrateEnable,
									uint16_t crcInitialValue,
									uint32_t meterLanSrcID,
									uint32_t meterLanDstID,
									uint8_t  packetTypeFilter,
									uint16_t packetLengthFilter)
{
    return gnuradio::make_block_sptr<GridStream_impl>(
        crcEnable, debugEnable, timestampEnable, epochEnable, frequencyEnable, baudrateEnable, 
        crcInitialValue, meterLanSrcID, meterLanDstID, packetTypeFilter, packetLengthFilter);
}


/*
 * The private constructor
 */
GridStream_impl::GridStream_impl(bool crcEnable,
								 bool debugEnable,
								 bool timestampEnable,
                                 bool epochEnable,
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
      d_epochEnable(epochEnable),
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

std::string time_in_HH_MM_SS_MMM()
{
    using namespace std::chrono;

    // get current time
    auto now = system_clock::now();

    // get number of milliseconds for the current second
    // (remainder after division into seconds)
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    // convert to std::time_t in order to convert to std::tm (broken time)
    auto timer = system_clock::to_time_t(now);

    // convert to broken time
    std::tm bt = *std::localtime(&timer);

    std::ostringstream oss;

    oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

uint16_t
GridStream_impl::crc16(uint16_t crc, const std::vector<uint8_t>& data, size_t packet_len, size_t header_len)
{
    // Some known CRC Init's below
    // uint16_t crc = 0x45F8;	// (CoServ CRC)
    // uint16_t crc = 0x5FD6;	// (Oncor CRC)
    // uint16_t crc = 0x62C1;	// (Hydro-Quebec CRC)
    uint16_t Poly = 0x1021;
    const int crc_len = 2;
    uint16_t i = header_len; // Skip over header/packet length [eg. 00,FF,2A,55,xx,(xx)]
    int size = packet_len - crc_len;
    while (size--) {
        crc ^= data[i] << 8;
        i++;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 0x8000 ? (crc << 1) ^ Poly : crc << 1;
    }
    return crc;
}

void GridStream_impl::pdu_handler(pmt::pmt_t pdu)
{
    pmt::pmt_t meta = pmt::car(pdu);
    pmt::pmt_t v_data = pmt::cdr(pdu);
	int header_len = 0;
	
    // make sure PDU data is formed properly
    if (!(pmt::is_pdu(pdu))) {
        GR_LOG_WARN(d_logger, "received unexpected PMT (non-pdu)");
        return;
    }
    size_t vlen = pmt::length(pmt::cdr(pdu));
    const std::vector<uint8_t> data = pmt::u8vector_elements(v_data);

    // Packet not large enough, probably noise
    if (data.size() < 6)
        return;

    // Packet decoded 00,FF,message_type,packet_type,packet_len
    int packet_type = data[3];
    int packet_len = 0;
    if (packet_type == 0xD2) {
        header_len = 5;
        packet_len = data[4];
    } else if ((packet_type == 0x55) || (packet_type == 0xD5)) {
        header_len = 6;
        packet_len = data[5];
    } else {
        packet_len = data.size();
    }

    int decoded_packet_size = header_len + packet_len;
    // Packet self reports larger than data we received
    if (decoded_packet_size > data.size()) {
        return;
    }
    
    const int min_packet_size = 4;
    // Packet self reports too small
    if (packet_len < min_packet_size) {
        return;
    }

    int receivedCRC = data[packet_len + header_len - 1] | data[packet_len + header_len - 2] << 8;
    uint16_t calculatedCRC = GridStream_impl::crc16(d_crcInitialValue, data, packet_len, header_len);

    uint32_t receivedMeterLanSrcID{ 0xFFFFFFFF };
    uint32_t receivedMeterLanDstID{ 0xFFFFFFFF };
    int upTime{ 0 };
	std::string GridStreamMeterSrcID{ "" };
	std::string GridStreamMeterDstID{ "" };
	
    if (receivedCRC == calculatedCRC) {
        if ( (packet_type == 0x55) && (packet_len == 0x0023) ) {
            receivedMeterLanSrcID = data[27 + 2] | 
                                    data[26 + 2] << 8 | 
                                    data[25 + 2] << 16 | 
                                    data[24 + 2] << 24;
            GridStreamMeterSrcID = (char_to_hex(int(data[26]))+
                                    char_to_hex(int(data[27]))+
                                    char_to_hex(int(data[28]))+
                                    char_to_hex(int(data[29])));
            GridStreamMeterDstID = "";
            upTime = data[21 + 2] | 
                    data[20 + 2] << 8 | 
                    data[19 + 2] << 16 | 
                    data[18 + 2] << 24;
        } 
        else if (packet_type == 0xD5) {
            receivedMeterLanSrcID = data[14] | 
                                    data[13] << 8 | 
                                    data[12] << 16 | 
                                    data[11] << 24;
            receivedMeterLanDstID = data[10] | 
                                    data[9] << 8 | 
                                    data[8] << 16 | 
                                    data[7] << 24;
            GridStreamMeterSrcID = (char_to_hex(int(data[11]))+
                                    char_to_hex(int(data[12]))+
                                    char_to_hex(int(data[13]))+
                                    char_to_hex(int(data[14])));    
            GridStreamMeterDstID = (char_to_hex(int(data[7]))+
                                    char_to_hex(int(data[8]))+
                                    char_to_hex(int(data[9]))+
                                    char_to_hex(int(data[10])));            
        }
    }

	double center_frequency = pmt::to_double(pmt::dict_ref(meta, pmt::intern("center_frequency"), pmt::PMT_NIL));
	int symbol_rate = pmt::to_double(pmt::dict_ref(meta, pmt::intern("symbol_rate"), pmt::PMT_NIL));
	
    std::time_t captured_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (pmt::dict_has_key(meta, pmt::intern("system_time"))) {
        captured_time = pmt::to_double(pmt::dict_ref(meta, pmt::intern("system_time"), pmt::PMT_NIL));
    }

    if (((receivedCRC == calculatedCRC) || !(d_crcEnable)) &&
        (((receivedMeterLanSrcID == d_meterLanSrcID) || (receivedMeterLanDstID == d_meterLanDstID)) ||
        ((d_meterLanDstID == 0) && (d_meterLanSrcID == 0))) &&
        ((packet_len == d_packetLengthFilter) || (d_packetLengthFilter == 0)) &&
        ((packet_type == d_packetTypeFilter) || (d_packetTypeFilter == 0))) 
        {
			if (d_debugEnable) {
				std::cout << std::setfill('0') << std::hex << std::setw(2) << std::uppercase;
				for (int i = 0; i < decoded_packet_size; i++) // +6 to include 00FF2Axxyyzz header  
					{
					std::cout << std::setw(2) << int(data[i]);
					}
				if (d_baudrateEnable) {
					std::cout << "\tBaudrate: " << std::dec << std::fixed << std::setprecision(0) << ((symbol_rate+25)/100)*100;			
				}
				if (d_frequencyEnable) {
					std::cout << "\tFreq: " << std::dec << std::fixed << std::setprecision(1) << floor(center_frequency/100000)/10;
				}
				if (!(d_crcEnable)) {
					if (receivedCRC == calculatedCRC) {
						std::cout << "\tCRC:OK";
					} else {
						std::cout << "\tCRC:BAD";
					}
				}
                if (d_epochEnable) {
                    std::cout << "\t" << time_in_HH_MM_SS_MMM();
                }
				if (d_timestampEnable) {
                    std::cout << "\t" << std::ctime(&captured_time);
				} else {
					std::cout << "\n";
				}
			}
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_LanSrcID"), pmt::mp(GridStreamMeterSrcID));
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_LanDstID"), pmt::mp(GridStreamMeterDstID));		
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_Uptime"), pmt::mp(upTime));
			meta = pmt::dict_add(meta, pmt::mp("Gridstream_Freq"), pmt::mp(double(floor(center_frequency/100000)/10)));
			
			message_port_pub(PMTCONSTSTR__PDU_OUT,(pmt::cons(meta, pmt::init_u8vector(data.size(), data))));
			return;
		} 
		else {
			return;
		}
}

} /* namespace smart_meters */
} /* namespace gr */
