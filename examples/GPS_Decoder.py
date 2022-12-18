#!/usr/bin/env python3

# Smart Meter GPS decoding script
# @BitBangingBytes
#
# Dumps data to console, copy and paste to a file and save as .csv
#
import socket, select, string, sys, binascii, struct, time
import numpy as np


def decodeGPS(encodedData):
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


if __name__ == "__main__":

    if len(sys.argv) < 2:
        print('')
        print('Usage : python3 GPS_Decoder.py "GPS String"')
        print('')
        print('eg.  ./GPS_Decoder.py 50C4EDDA2B5F')
        print('')
        sys.exit()

    gpsData = sys.argv[1]

    gpsBytes = bytes.fromhex(gpsData)
    if gpsBytes[0] == 0xFE:
        print('Device not using GPS coordinates for WAN address.')
        sys.exit()
    GPS_Lat, GPS_Lon, Color = decodeGPS(gpsBytes)
    print(str(round(GPS_Lat, 6)) + "," + str(round(GPS_Lon, 6)))
