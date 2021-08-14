/* -*- c++ -*- */
/*
 * Copyright 2018, 2019, 2020 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government
 * retains certain rights in this software.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef INCLUDED_SMART_METERS_CONSTANTS_H
#define INCLUDED_SMART_METERS_CONSTANTS_H

#include <pmt/pmt.h>
#include <smart_meters/api.h>

namespace gr {
namespace smart_meters {

static const uint32_t BIT_ORDER_MSB_FIRST = 0;
static const uint32_t BIT_ORDER_LSB_FIRST = 1;

// TODO: Change these to enums?
static const uint32_t MODE_UNPACK_BYTE = 0;
static const uint32_t MODE_PACK_BYTE = 1;
static const uint32_t MODE_BITSWAP_BYTE = 2;

// TODO: Change these to enums?
static const uint32_t INPUTTYPE_UNPACKED_BYTE = 0;
static const uint32_t INPUTTYPE_PACKED_BYTE = 1;
static const uint32_t INPUTTYPE_FLOAT = 1;

static const uint32_t EARLY_BURST_BEHAVIOR__APPEND = 0;
static const uint32_t EARLY_BURST_BEHAVIOR__DROP = 1;
static const uint32_t EARLY_BURST_BEHAVIOR__BALK = 2;

// message port names
static const pmt::pmt_t PMTCONSTSTR__MSG = pmt::mp("msg");
static const pmt::pmt_t PMTCONSTSTR__OUT = pmt::mp("out");
static const pmt::pmt_t PMTCONSTSTR__IN = pmt::mp("in");
static const pmt::pmt_t PMTCONSTSTR__PDU_OUT = pmt::mp("pdu_out");
static const pmt::pmt_t PMTCONSTSTR__PDU_IN = pmt::mp("pdu_in");
static const pmt::pmt_t PMTCONSTSTR__DICT = pmt::mp("dict");
static const pmt::pmt_t PMTCONSTSTR__DATA = pmt::mp("data");
static const pmt::pmt_t PMTCONSTSTR__EMIT = pmt::mp("emit");
static const pmt::pmt_t PMTCONSTSTR__HEAD = pmt::mp("head");
static const pmt::pmt_t PMTCONSTSTR__TAIL = pmt::mp("tail");
static const pmt::pmt_t PMTCONSTSTR__CTRL = pmt::mp("ctrl");
static const pmt::pmt_t PMTCONSTSTR__CONF = pmt::mp("conf");
static const pmt::pmt_t PMTCONSTSTR__BURSTS = pmt::mp("bursts");
static const pmt::pmt_t PMTCONSTSTR__DETECTS = pmt::mp("detects");
static const pmt::pmt_t PMTCONSTSTR__DEBUG = pmt::mp("debug");
static const pmt::pmt_t PMTCONSTSTR__ZEROX = pmt::mp("zeroX");
static const pmt::pmt_t PMTCONSTSTR__WINDOW = pmt::mp("window");
static const pmt::pmt_t PMTCONSTSTR__BURST_ID = pmt::mp("burst_id");

static const pmt::pmt_t PMTCONSTSTR__EOB_OFFSET = pmt::mp("eob_offset");
static const pmt::pmt_t PMTCONSTSTR__EOB_ALIGNMENT = pmt::mp("eob_alignment");
static const pmt::pmt_t PMTCONSTSTR__RX_TIME = pmt::mp("rx_time");
static const pmt::pmt_t PMTCONSTSTR__SAMP_RATE = pmt::mp("sample_rate");
static const pmt::pmt_t PMTCONSTSTR__SYM_RATE = pmt::mp("symbol_rate");

static const pmt::pmt_t PMTCONSTSTR__BURST_TIME = pmt::mp("burst_time");
static const pmt::pmt_t PMTCONSTSTR__START_TIME = pmt::mp("start_time");
static const pmt::pmt_t PMTCONSTSTR__START_TIME_OFFSET = pmt::mp("start_time_offset");
static const pmt::pmt_t PMTCONSTSTR__TX_TIME = pmt::mp("tx_time");
static const pmt::pmt_t PMTCONSTSTR__TX_SOB = pmt::mp("tx_sob");
static const pmt::pmt_t PMTCONSTSTR__TX_EOB = pmt::mp("tx_eob");
static const pmt::pmt_t PMTCONSTSTR__PDU_NUM = pmt::mp("pdu_num");
static const pmt::pmt_t PMTCONSTSTR__TIME_TYPE = pmt::mp("time_type");
static const pmt::pmt_t PMTCONSTSTR__UHD_TIME_TUPLE = pmt::mp("uhd_time_tuple");
static const pmt::pmt_t PMTCONSTSTR__WALL_CLOCK_TIME = pmt::mp("wall_clock_time");
static const pmt::pmt_t PMTCONSTSTR__DURATION = pmt::mp("duration");

// enumerate command port commands:
static const pmt::pmt_t PMTCONSTSTR__SET_TRIGGER_KEY =
    pmt::string_to_symbol("set_trigger_tag");
static const pmt::pmt_t PMTCONSTSTR__SET_ARMING_KEY =
    pmt::string_to_symbol("set_arming_tag");
static const pmt::pmt_t PMTCONSTSTR__SET_TX_LIMIT = pmt::string_to_symbol("set_tx_limit");
static const pmt::pmt_t PMTCONSTSTR__SET_DELAYS = pmt::string_to_symbol("set_delays");
static const pmt::pmt_t PMTCONSTSTR__SET_MESSAGE = pmt::string_to_symbol("set_message");
static const pmt::pmt_t PMTCONSTSTR__SET_HOLDOFF = pmt::string_to_symbol("set_holdoff");
static const pmt::pmt_t PMTCONSTSTR__SET_ARMED = pmt::string_to_symbol("set_armed");
static const pmt::pmt_t PMTCONSTSTR__TRIGGER_NOW = pmt::string_to_symbol("trigger_now");

static const uint32_t MODE_MESSAGE = 0;
static const uint32_t MODE_TIMED_PDU = 1;

static const uint64_t TX_UNLIMITED = 0xFFFFFFFFFFFFFFFF; // = -1
static const uint64_t TX_OFF = 0;                        // no transmissions


// noise generation distributions
enum noise_dist { UNIFORM = 0, GAUSSIAN = 1 };

enum window_type { TUKEY_WIN = 0, GAUSSIAN_WIN = 1 }; // end enum window_type

//! pdu align modes
enum align_modes {
    //! do not emit a packet if sync word is not found
    ALIGN_DROP = 0,

    //! forward packet if sync word is not found
    ALIGN_FORWARD = 1,

    //! emit empty pdu if sync word is not found
    ALIGN_EMPTY = 2
};


} // namespace smart_meters
} // namespace gr

#endif /* INCLUDED_PDU_UTILS_CONSTANTS_H */
