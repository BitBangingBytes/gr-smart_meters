#!/usr/bin/env python
# -*- coding: utf-8 -*-
'''
 Copyright 2021, 2022 Hash.

 SPDX-License-Identifier: GPL-3.0-or-later
'''

from gnuradio import gr, gr_unittest
from gnuradio import blocks
from gnuradio import pdu_utils
import pmt
import time

try:
    from smart_meters import GridStream
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from smart_meters import GridStream

class qa_GridStream(gr_unittest.TestCase):

    def setUp(self):
        self.tb = gr.top_block()
        self.emitter = pdu_utils.message_emitter()
        self.debug = blocks.message_debug()
        self.in_meta = pmt.make_dict()
        self.expected_meta = pmt.make_dict()
        self.expected_pdu = 0

    def connectUp(self):
        self.tb.msg_connect((self.emitter, 'msg'), (self.block_under_test, 'pdu_in'))
        self.tb.msg_connect((self.block_under_test, 'pdu_out'), (self.debug, 'store'))

    def tearDown(self):
        self.tb = None

    def test_001_complete_packet_pass_through(self):
        # GridStream(eCRC, eDEBUG, eTimeStamp, eEpoch, eFrequency, eBaudRate, 
        #            crcInit, LANsrc, LANdst, FilterPktType, FilterPktLen)
        self.block_under_test = GridStream(False, False, False, False, False, False, 
                                           0x5FD6, 0, 0, 0x00, 0x00)
        self.connectUp()

        in_data = [0x00, 0xff, 0x2a, 0x55, 0x00, 0x23, 0x30, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x50, 0xcf, 0xfd, 
                   0xd9, 0xe4, 0xe0, 0xb2, 0x01, 0x0f, 0x27, 0x48, 0xa4, 0x83, 0xf0, 0x6e, 0xbc, 0x01, 0x01, 0x00, 
                   0x18, 0x66, 0x03, 0x39, 0xf3, 0x7e, 0x90, 0xdc, 0xdb, 0x04]
        expected_data = in_data

        self.in_meta = pmt.dict_add(self.in_meta, pmt.intern("center_frequency"), pmt.from_double(915300000))
        in_pdu = pmt.cons(self.in_meta, pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("center_frequency"), pmt.from_double(915300000))
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("Gridstream_LanSrcID"), pmt.intern("F06EBC01"))
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("Gridstream_WanSrcID"), pmt.intern("50CFFDD9E4E0"))
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("Gridstream_LanDstID"), pmt.intern(""))
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("Gridstream_Uptime"), pmt.from_long(17770312))
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("Gridstream_Freq"), pmt.from_double(915.3))

        self.expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), self.expected_pdu))

    def test_002_partial_packet_pass_through(self):
        # GridStream(eCRC, eDEBUG, eTimeStamp, eEpoch, eFrequency, eBaudRate, 
        #            crcInit, LANsrc, LANdst, FilterPktType, FilterPktLen)
        self.block_under_test = GridStream(False, False, False, False, False, False, 
                                           0x5FD6, 0, 0, 0x00, 0x00)
        self.connectUp()

        in_data = [0x00, 0xff, 0x2a, 0x55, 0x00, 0x23, 0x30, 0xff]
        expected_data = in_data

        self.in_meta = pmt.dict_add(self.in_meta, pmt.intern("center_frequency"), pmt.from_double(915300000))
        in_pdu = pmt.cons(self.in_meta, pmt.init_u8vector(len(in_data), in_data))
        
        expected_pdu = pmt.cons(self.in_meta, pmt.init_u8vector(len(expected_data), expected_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(in_pdu, expected_pdu))

    def test_003_passing_crc_pass_through(self):
        # GridStream(eCRC, eDEBUG, eTimeStamp, eEpoch, eFrequency, eBaudRate, 
        #            crcInit, LANsrc, LANdst, FilterPktType, FilterPktLen)
        self.block_under_test = GridStream(True, False, False, False, False, False, 
                                           0x5FD6, 0, 0, 0x00, 0x00)
        self.connectUp()

        in_data = [0x00, 0xff, 0x2a, 0x55, 0x00, 0x23, 0x30, 0xff, 
                   0xff, 0xff, 0xff, 0xff, 0xff, 0x50, 0xcf, 0xfd, 
                   0xd9, 0xe4, 0xe0, 0xb2, 0x01, 0x0f, 0x27, 0x48, 
                   0xa4, 0x83, 0xf0, 0x6e, 0xbc, 0x01, 0x01, 0x00, 
                   0x18, 0x66, 0x03, 0x39, 0xf3, 0x7e, 0x90, 0xdc, 
                   0xdb, 0x04]
        expected_data = in_data

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        data = pmt.cdr(self.debug.get_message(0))

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(data, expected_data)

    def test_004_failing_crc_no_pass_through(self):
        # GridStream(eCRC, eDEBUG, eTimeStamp, eEpoch, eFrequency, eBaudRate, 
        #            crcInit, LANsrc, LANdst, FilterPktType, FilterPktLen)
        self.block_under_test = GridStream(True, False, False, False, False, False, 
                                           0x5FD6, 0, 0, 0x00, 0x00)
        self.connectUp()

        in_data = [0x00, 0xff, 0x2a, 0x55, 0x00, 0x23, 0x30, 0xff, 
                   0xff, 0xff, 0xff, 0xff, 0xff, 0x50, 0xcf, 0xfd, 
                   0xd9, 0xe4, 0xe0, 0xb2, 0x01, 0x0f, 0x27, 0x48, 
                   0xa4, 0x83, 0xf0, 0x6e, 0xbc, 0x01, 0x01, 0x00, 
                   0x18, 0x66, 0x03, 0x39, 0xf3, 0x7e, 0x90, 0xaa,  #Changed 0xdc to 0xaa 
                   0xdb, 0x04]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(0, self.debug.num_messages())
    
    def test_005_failing_crc_partial_packet_no_pass_through(self):
        # GridStream(eCRC, eDEBUG, eTimeStamp, eEpoch, eFrequency, eBaudRate, 
        #            crcInit, LANsrc, LANdst, FilterPktType, FilterPktLen)
        self.block_under_test = GridStream(True, False, False, False, False, False, 
                                           0x5FD6, 0, 0, 0x00, 0x00)
        self.connectUp()

        in_data = [0x00, 0xff, 0x2a, 0x55, 0x00, 0x23, 0x30, 0xff, 
                   0xff, 0xff, 0xff, 0xff, 0xff, 0x50, 0xcf, 0xfd, 
                   0xd9, 0xe4, 0xe0, 0xb2, 0x01, 0x0f, 0x27, 0x48, 
                   0xa4, 0x83]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(0, self.debug.num_messages())

if __name__ == '__main__':
    gr_unittest.run(qa_GridStream)
