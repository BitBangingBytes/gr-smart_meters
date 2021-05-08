# Smart Meter GPS decoding script
# @BitBangingBytes
#
# Dumps data to console, copy and paste to a file and save as .csv
#
import socket, select, string, sys, binascii, struct, time
import numpy as np

def decodeGPS(encodedData):
	latMultiplier = (float(2**20)/90)
	lonMultiplier = (float(2**20)/180)
	latNeg = bool(int(binascii.hexlify(encodedData),16) & 0x800000000000)
	lonNeg = bool(int(binascii.hexlify(encodedData),16) & 0x000004000000)
	
	latEncoded = (int(binascii.hexlify(encodedData),16) & 0x7FFFF8000000) >> 27
	lonEncoded = (int(binascii.hexlify(encodedData),16) & 0x000003FFFFC0) >> 6
	color = (int(binascii.hexlify(encodedData),16) & 0x00000000001F)
	
	if (latNeg):
		lat = (-1 * (latEncoded / latMultiplier))
	else:
		lat = (90 - (latEncoded / latMultiplier))
	
	if (lonNeg):
		lon = ((lonEncoded / lonMultiplier) - 180)
	else:
		lon = (lonEncoded / lonMultiplier)
	
	return lat, lon, color

#main function
if __name__ == "__main__":
	
	if(len(sys.argv) < 3) :
		print 'Usage : python SmartMeterGPSDecoder-v1.py hostname port'
		sys.exit()
	
	sdrHost = sys.argv[1]
	sdrPort = int(sys.argv[2])

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.settimeout(2)
	# connect to remote host
	try :
		s.connect_ex((sdrHost, sdrPort))
	except :
		print 'Unable to connect to flowgraph'
		sys.exit()
	print 'Connected to remote SDR flowgraph'

	while 1:
		sdr_socket_list = [sys.stdin, s]
		
		# Get the list sockets which are readable
		sdr_read_sockets, sdr_write_sockets, sdr_error_sockets = select.select(sdr_socket_list , [], [])
		
		for sock in sdr_read_sockets:
			#incoming message from remote server
			if sock == s:
				sdrData = sock.recv(4096)
				if not sdrData :
					print 'Connection closed'
					sys.exit()
				else :
					#print("The original string is : " + str(sdrData))
					#Do nothing for the Non-GPS data
					if ((binascii.hexlify(bytearray(sdrData[3])) == "55") and (binascii.hexlify(bytearray(sdrData[13])) == "fe")) : #Non-GPS Routed Data
						upTime = int(binascii.hexlify(bytearray(sdrData[20:24])),16)
						meterID = bytearray(sdrData[26:30])
						#print (binascii.hexlify(meterID).upper() + "," + str(upTime) + "," + str(upTime/60/60/24))
					#Decode GPS data
					elif (binascii.hexlify(bytearray(sdrData[3])) == "55" and (binascii.hexlify(bytearray(sdrData[13])) != "fe")) : #GPS Routed Data
						GPS_Lat, GPS_Lon, Color = decodeGPS(sdrData[13:19])
						upTime = int(binascii.hexlify(bytearray(sdrData[20:24])),16)
						meterID = bytearray(sdrData[26:30])
						epoch = time.time()
						formatted_time = "{:.6f}".format(epoch)						
						print (binascii.hexlify(meterID).upper() + "," + str(upTime) + "," + str(upTime/60/60/24) + "," + str(round(GPS_Lat,6)) + "," + str(round(GPS_Lon,6))) + "," + str(formatted_time)
			else :
				msg = sys.stdin.readline()
				s.send(msg)



