#!/usr/bin/env python
# -*- coding: utf-8 -*-
'''
 Copyright 2021, 2022 Hash.

 SPDX-License-Identifier: GPL-3.0-or-later
'''

from gnuradio import gr
from gnuradio import pdu_utils
import pmt
import binascii
import simplekml

class google_earth(gr.sync_block):
    """
    docstring for block google_earth
    """
    def __init__(self, file_save="./smart_meters.kml"):
        gr.sync_block.__init__(self,
            name="Google Earth",
            in_sig=None,
            out_sig=None)
        
        self.log = gr.logger('log')
        self.file_save = file_save

        self.message_port_register_in(pdu_utils.PMTCONSTSTR__pdu_in())
        self.set_msg_handler(pdu_utils.PMTCONSTSTR__pdu_in(), self.handle_pdu)
        self.message_port_register_out(pdu_utils.PMTCONSTSTR__pdu_out())

        # pre-define PDU keys
        self.gridstream_lan_src_key = pmt.intern("Gridstream_LanSrcID")
        self.gridstream_lan_dst_key = pmt.intern("Gridstream_LanDstID")
        self.gridstream_wan_src_key = pmt.intern("Gridstream_WanSrcID")
        self.gridstream_uptime = pmt.intern("Gridstream_Uptime")

        self.kml = simplekml.Kml()
        self.style = simplekml.Style()
        self.style.iconstyle.icon.href = 'http://maps.google.com/mapfiles/kml/shapes/placemark_circle_highlight.png'
        
        self.meters_list = []
        self.meter_location = []

    def handle_pdu(self, pdu):
        # if PDU is not pair, drop and wait for new PDU
        if not pmt.is_pair(pdu):
            self.log.warn("PDU is not a pair, dropping")
            return
        
        meta = pmt.car(pdu)
        lan_src = ""
        wan_src = ""
        uptime = 0

        # if PDU has GridStream Src Key in the metadata, try to use that value
        if pmt.is_dict(meta) and pmt.dict_has_key(meta, self.gridstream_lan_src_key):
            lan_src = pmt.to_python(pmt.dict_ref(meta, self.gridstream_lan_src_key, pmt.mp(self.gridstream_lan_src_key)))

        if pmt.is_dict(meta) and pmt.dict_has_key(meta, self.gridstream_wan_src_key):
            wan_src = pmt.to_python(pmt.dict_ref(meta, self.gridstream_wan_src_key, pmt.mp(self.gridstream_wan_src_key)))

        if pmt.is_dict(meta) and pmt.dict_has_key(meta, self.gridstream_uptime):
            uptime = pmt.to_python(pmt.dict_ref(meta, self.gridstream_uptime, pmt.mp(self.gridstream_uptime)))
            uptime = int(uptime/60/60/24)
            uptime_plot_height_meters = uptime/10

        self.lat, self.lon, color = self.decodeGPS(bytes.fromhex(wan_src))
        meter_lan_id = lan_src

        name = 0
        point = 1
        for self.meter_location in self.meters_list:
            if self.meter_location[name] == meter_lan_id:
                break
            else:
                self.meter_location = []

        if self.meter_location == []:
            pnt = self.kml.newpoint(name=meter_lan_id, coords=[(self.lon, self.lat, uptime_plot_height_meters)])
            pnt.style = self.style
            pnt.altitudemode = simplekml.AltitudeMode.relativetoground
            pnt.extrude = True
            pnt.linestyle.width = 8
            pnt.description = str(uptime) + " days"
            self.meters_list.append([meter_lan_id, pnt])

        if self.meter_location:
            self.meter_location[point].coords = [(self.lon, self.lat, uptime_plot_height_meters)]
            self.meter_location[point].description = str(uptime) + " days"

        self.kml.save(self.file_save)

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
