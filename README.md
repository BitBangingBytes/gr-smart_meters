# gr-smart_meters
gr-smart_meters is a GNU Radio out-of-tree module meant to contain various decoders for smart meter manufacturers. 
The initial release contains a decoder for GridStream used by Landis+Gyr.
# Documentation
A guide on what is needed to get this running is available on the RECESSIM Wiki. The decoder is built off of gr-FHSS_Utils by Sandia Labs. https://wiki.recessim.com/view/Gr-smart_meters_Setup_Guide
# Known CRC Values
Austin, TX.   |   Austin Energy       |   CRC = 0xD553

Dallas, TX    |   CoServ              |   CRC = 0x45F8

Dallas, TX    |   Oncor               |   CRC = 0x5FD6

Quebec, CAN   |   Hydro-Quebec        |   CRC = 0x62C1

Seattle, WA   |   Seattle City Light  |   CRC = 0x23D1

Santa Barbara, CA   |   Southern California Edison   |   CRC = 0x2C22

Washington   |   Puget Sound Energy   |   CRC = 0x142A

Kansas City  |   Evergy               |   CRC = 0xE623
