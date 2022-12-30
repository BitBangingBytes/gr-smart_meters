#!/usr/bin/env python
# -*- coding: utf-8 -*-
'''
 Copyright 2021, 2022 Hash.

 SPDX-License-Identifier: GPL-3.0-or-later
'''

from gnuradio import gr
from gnuradio import pdu_utils
import pmt
import gmplot
import webbrowser
import binascii

class google_map(gr.sync_block):
    """
    docstring for block google_map
    """
    def __init__(self, API_Key="", file_save="./smart_meters.html", Start_Lat=0, Start_Lon=0, Start_Zoom=15):
        gr.sync_block.__init__(self,
            name="Google Map",
            in_sig=None,
            out_sig=None)
        
        self.log = gr.logger('log')

        self.message_port_register_in(pdu_utils.PMTCONSTSTR__pdu_in())
        self.set_msg_handler(pdu_utils.PMTCONSTSTR__pdu_in(), self.handle_pdu)
        self.message_port_register_out(pdu_utils.PMTCONSTSTR__pdu_out())

        # pre-define PDU keys
        self.gridstream_lan_src_key = pmt.intern("Gridstream_LanSrcID")
        self.gridstream_wan_src_key = pmt.intern("Gridstream_WanSrcID")

        self.API_Key = API_Key
        self.file_save = file_save
        self.lat = Start_Lat
        self.lon = Start_Lon
        self.zoom = Start_Zoom

        self.gmap = gmplot.GoogleMapPlotter(self.lat, self.lon, self.zoom, apikey=self.API_Key, map_type="satellite")
        self.gmap.draw(self.file_save)

        # Uncomment this to automatically open a browser window
        # webbrowser.open_new_tab(self.file_save)

    def handle_pdu(self, pdu):
        # if PDU is not pair, drop and wait for new PDU
        if not pmt.is_pair(pdu):
            self.log.warn("PDU is not a pair, dropping")
            return
        meta = pmt.car(pdu)
        lan_src = ""
        wan_src = ""

        # if PDU has GridStream Src Key in the metadata, try to use that value
        if pmt.is_dict(meta) and pmt.dict_has_key(meta, self.gridstream_lan_src_key):
            lan_src = pmt.to_python(pmt.dict_ref(meta, self.gridstream_lan_src_key, pmt.mp(self.gridstream_lan_src_key)))

        if pmt.is_dict(meta) and pmt.dict_has_key(meta, self.gridstream_wan_src_key):
            wan_src = pmt.to_python(pmt.dict_ref(meta, self.gridstream_wan_src_key, pmt.mp(self.gridstream_wan_src_key)))

        self.lat, self.lon, color = self.decodeGPS(bytes.fromhex(wan_src))
        meter_lan_id = lan_src
        self.gmap.marker(self.lat, self.lon, title=meter_lan_id)
        self.gmap.draw(self.file_save)

    def decodeGPS(self, encodedData):
        latMultiplier = (float(2 ** 20) / 90)
        lonMultiplier = (float(2 ** 20) / 180)
        latNeg = bool(int(binascii.hexlify(encodedData), 16) & 0x800000000000)
        lonNeg = bool(int(binascii.hexlify(encodedData), 16) & 0x000004000000)

        latEncoded = (int(binascii.hexlify(encodedData), 16) & 0x7FFFF8000000) >> 27
        lonEncoded = (int(binascii.hexlify(encodedData), 16) & 0x000003FFFFC0) >> 6
        color = (int(binascii.hexlify(encodedData), 16) & 0x00000000001F)

        if latNeg:
            lat = (-1 * (latEncoded / latMultiplier))
        else:
            lat = (90 - (latEncoded / latMultiplier))

        if lonNeg:
            lon = ((lonEncoded / lonMultiplier) - 180)
        else:
            lon = (lonEncoded / lonMultiplier)

        return lat, lon, color
