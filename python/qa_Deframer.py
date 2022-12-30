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
    from smart_meters import Deframer
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from smart_meters import Deframer

class qa_Deframer(gr_unittest.TestCase):

    def setUp(self):
        self.tb = gr.top_block()
        self.emitter = pdu_utils.message_emitter()
        self.debug = blocks.message_debug()
        self.expected_meta = pmt.make_dict()

    def connectUp(self):
        self.tb.msg_connect((self.emitter, 'msg'), (self.block_under_test, 'pdu_in'))
        self.tb.msg_connect((self.block_under_test, 'pdu_out'), (self.debug, 'store'))

    def tearDown(self):
        self.tb = None

    def test_000_frame_pass_gridstream_return_version_0(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        expected_data = [ 0x08, 0xFF, 0x00 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(0))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_001_frame_pass_header_plus_one_byte_gridstream_version_4(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        expected_data = [ 0x00, 0xFF, 0x00 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(4))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_002_frame_pass_header_plus_one_byte_gridstream_version_5(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        expected_data = [ 0x80, 0xFF, 0x00 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(5))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()
        # print(self.debug.get_message(0), expected_pdu)
        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_003_frame_fail_deframer_minimum_length(self):
        self.block_under_test = Deframer(4, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(0, self.debug.num_messages())

    def test_004_frame_fail_deframer_maximum_length(self):
        self.block_under_test = Deframer(1, 2, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(0, self.debug.num_messages())

    def test_005_frame_pass_startbit_fail_exit_early(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        expected_data = [ 0x00, 0x00 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(0))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           


        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_006_frame_minimum_lenght_must_be_two_bytes(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(0, self.debug.num_messages())
    
    def test_007_frame_pass_single_frame_error_gridstream_version_4(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  # Extra bit
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1]
        expected_data = [ 0x00, 0xFF, 0x00, 0x00 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))
        
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(4))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_008_frame_pass_double_frame_error_gridstream_version_4(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  # Extra bit
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  # Extra bit
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,]
        expected_data = [ 0x00, 0xFF, 0x00, 0x00, 0xFF ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))
        
        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(4))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_009_frame_pass_another_single_frame_error_gridstream_version_4(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
                   0, 0, 1, 0, 1, 0, 1, 0, 0, 1,     
                   0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   0, 1, 0, 1, 0, 1, 0, 1, 1, 1]     
        expected_data = [ 0x00, 0xFF, 0x2A, 0x55, 0x00, 0xFF, 0xD5 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(4))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_010_frame_pass_GridStream_V5_start_of_frame(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  
                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
                   0, 0, 1, 0, 1, 0, 1, 0, 1, 1,     
                   0, 1, 0, 1, 0, 1, 0, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
                   0, 0, 1, 0, 0, 0, 1, 0, 0, 1]     
        expected_data = [ 0x80, 0xFF, 0xAA, 0xD5, 0x00, 0x21, 0x22 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(5))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.01)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")

    def test_011_frame_pass_GridStream_V5_multiple_packets_in_burst(self):
        self.block_under_test = Deframer(1, 300, False)
        self.connectUp()

        in_data = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  
                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
                   0, 0, 1, 0, 1, 0, 1, 0, 1, 1,     
                   0, 1, 0, 1, 0, 1, 0, 1, 1, 1,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
                   0, 0, 1, 0, 0, 0, 1, 0, 0, 1,
                   0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                   1,
                   0, 1, 0, 1, 0, 1, 0, 0, 0, 1,
                   0, 0, 0, 0, 1, 0, 0, 0, 1, 1,
                   0, 1, 1, 0, 0, 1, 1, 1, 1, 1]     
                   
        expected_data = [ 0x80, 0xFF, 0xAA, 0xD5, 0x00, 0x21, 0x22, 0x00, 0xFF, 0x15, 0x88, 0xF3 ]

        in_pdu = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(in_data), in_data))

        self.expected_meta = pmt.dict_add(self.expected_meta, pmt.intern("GridStream_Version"), pmt.mp(5))
        expected_pdu = pmt.cons(self.expected_meta, pmt.init_u8vector(len(expected_data), expected_data))                           

        self.tb.start()
        time.sleep(.001)
        self.emitter.emit(in_pdu)
        time.sleep(.05)
        self.tb.stop()
        self.tb.wait()

        self.assertEqual(1, self.debug.num_messages())
        self.assertTrue(pmt.equal(self.debug.get_message(0), expected_pdu), f"\nActual:   {self.debug.get_message(0)}\nExpected: {expected_pdu}\n")


if __name__ == '__main__':
    gr_unittest.run(qa_Deframer)
