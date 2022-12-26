#!/bin/python3

# Copyright 2022 Hash @BitBangingBytes (GPL3)
#
# Program to convert from GridStream CRC back to the NetworkID used by Landis+Gyr
#

import sys
from binascii import crc_hqx as crc16


def get_seed(arg):
    for i in range(0x0000, 0x10000):
        seed = crc16(i.to_bytes(2, 'big'), 0)
        if seed == arg:
            return i


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("")
        print("Enter the Landis+Gyr GridStream CRC (eg. 0x5FD6) to convert it to the NetworkID.")
        print("")
        print("python3 GridStream_NetworkID.py 0x5FD6")
        print("")
        exit()

    hex_crc = {}
    network_id = {}
    try:
        hex_crc = int(sys.argv[1], 16)
        if hex_crc < 0x0000 or hex_crc > 0xFFFF:
            raise ValueError
        network_id = get_seed(hex_crc)
    except ValueError:
        print("\nERROR: Please enter a hex value between 0x0000 and 0xFFFF\n")
        exit()

    print(f"\nCRC: 0x{hex_crc:04X} is Network ID: {network_id}  (0x{network_id:04X})\n")
