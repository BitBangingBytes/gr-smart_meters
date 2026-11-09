#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
"""Deframer tolerant_framing and GridStream crcCorrectBits, end to end on synthetic v4 frames."""

import time

import pmt
from gnuradio import blocks, gr, gr_unittest
from gnuradio import pdu_utils

try:
    from smart_meters import Deframer, GridStream
except ImportError:
    import os
    import sys
    sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "bindings"))
    from smart_meters import Deframer, GridStream

CRC_INIT = 0x0415      # unprovisioned NetID 0x029E


def crc16(data, init=CRC_INIT):
    c = init
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def frame():
    body = bytes.fromhex("30fffffffffffffe00000000001112131415161790000001000000000000000000")
    c = crc16(body)
    return bytes([0x2A, 0x55, 0x00, len(body) + 2]) + body + bytes([c >> 8, c & 0xFF])


def to_bits(onair):
    """v4 SOF (0xFFA0 as the Deframer reads it), then UART bytes: 0, 8 data bits LSB first, 1."""
    bits = [0, 0, 0, 0] + [(0xFFA0 >> k) & 1 for k in range(16)]
    for b in onair:
        bits += [0] + [(b >> k) & 1 for k in range(8)] + [1]
    return bits + [1] * 40


def uart_bit(byte_index, k):
    """Index in to_bits() of data bit k (LSB first) of on-air byte byte_index."""
    return 20 + 10 * byte_index + 1 + k


class qa_robust_decode(gr_unittest.TestCase):

    def run_chain(self, bits, tolerant, correct):
        tb = gr.top_block()
        emitter = pdu_utils.message_emitter()
        dfr = Deframer(6, 300, False, False, tolerant)
        gs = GridStream(False, False, False, False, False, False, False, CRC_INIT, 0, 0, 0, 0, correct)
        dbg = blocks.message_debug()
        tb.msg_connect((emitter, "msg"), (dfr, "pdu_in"))
        tb.msg_connect((dfr, "pdu_out"), (gs, "pdu_in"))
        tb.msg_connect((gs, "pdu_out"), (dbg, "store"))
        tb.start()
        time.sleep(0.01)
        emitter.emit(pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(bits), bits)))
        time.sleep(0.05)
        tb.stop()
        tb.wait()
        self.assertEqual(dbg.num_messages(), 1)
        msg = dbg.get_message(0)
        meta = pmt.to_python(pmt.car(msg))
        data = bytes(pmt.u8vector_elements(pmt.cdr(msg)))
        return meta, data

    def test_clean_frame_all_modes(self):
        f = frame()
        for tol in (False, True):
            for cor in (0, 1):
                meta, data = self.run_chain(to_bits(f), tol, cor)
                self.assertEqual(meta["gs_crc"], "OK")
                self.assertEqual(data[2:2 + len(f)], f)
                self.assertNotIn("gs_crc_fixed", meta)

    def test_flipped_stop_bit(self):
        f = frame()
        bits = to_bits(f)
        bits[20 + 10 * 12 + 9] ^= 1                 # stop bit of on-air byte 12
        meta, _ = self.run_chain(bits, False, 0)
        self.assertEqual(meta["gs_crc"], "BAD")     # stock: frame truncated at byte 12
        meta, data = self.run_chain(bits, True, 0)
        self.assertEqual(meta["gs_crc"], "OK")      # tolerant: the data bits were fine
        self.assertEqual(data[2:2 + len(f)], f)

    def test_flipped_data_bit(self):
        f = frame()
        bits = to_bits(f)
        bits[uart_bit(20, 3)] ^= 1                  # data bit 3 (LSB first) of on-air byte 20
        meta, _ = self.run_chain(bits, True, 0)
        self.assertEqual(meta["gs_crc"], "BAD")
        meta, data = self.run_chain(bits, True, 1)
        self.assertEqual(meta["gs_crc"], "OK")
        self.assertEqual(meta["gs_crc_fixed"], (2 + 20) * 8 + (7 - 3))
        self.assertEqual(data[2:2 + len(f)], f)     # repaired bytes go downstream

    def test_flipped_crc_bit(self):
        f = frame()
        bits = to_bits(f)
        bits[uart_bit(len(f) - 1, 0)] ^= 1          # in the CRC itself
        meta, data = self.run_chain(bits, True, 1)
        self.assertEqual(meta["gs_crc"], "OK")
        self.assertEqual(data[2:2 + len(f)], f)

    def test_two_data_bits_not_repaired(self):
        f = frame()
        bits = to_bits(f)
        bits[uart_bit(10, 1)] ^= 1
        bits[uart_bit(25, 6)] ^= 1
        meta, _ = self.run_chain(bits, True, 1)
        self.assertEqual(meta["gs_crc"], "BAD")
        self.assertNotIn("gs_crc_fixed", meta)

    def test_framing_and_data_error_together(self):
        f = frame()
        bits = to_bits(f)
        bits[20 + 10 * 7] ^= 1                      # start bit of byte 7
        bits[uart_bit(30, 5)] ^= 1                  # and a data bit of byte 30
        meta, data = self.run_chain(bits, True, 1)
        self.assertEqual(meta["gs_crc"], "OK")
        self.assertEqual(data[2:2 + len(f)], f)

    def test_tolerant_stops_at_length(self):
        f = frame()
        bits = to_bits(f)[:-40] + [0] * 60          # garbage (all-zero, unframed) after the frame
        meta, data = self.run_chain(bits, True, 0)
        self.assertEqual(meta["gs_crc"], "OK")
        self.assertEqual(len(data), 2 + len(f))     # nothing past the announced length


if __name__ == "__main__":
    gr_unittest.run(qa_robust_decode)
