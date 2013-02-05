#---------------------------------------------------------------------------
#                                 OPERA
#---------------------------------------------------------------------------
# Author: Cedric Adjih
# Copyright 2011 Inria.
#
# This file is part of the OPERA.
#
# The OPERA is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# The OPERA is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with the OPERA; see the file LICENSE.LGPLv3.  If not, see
# http://www.gnu.org/licenses/ or write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA. 
#---------------------------------------------------------------------------

"""
  This program forwards the packets received on a port to another port.
  The goal of the program is to forwards packet received from the 
TI Packet Sniffer to another machine. This is specially useful when the
code is run in a guest Windows virtual machine, and the packet processing is
made on the Linux host.
  The program also dumps the packets as hexadecimal bytes.
"""

import socket, time, struct

def popStruct(spec, data):
    specSize = struct.calcsize(spec)
    return struct.unpack(spec, data[:specSize]), data[specSize:]

def displayPacket(packetIndex, timestamp, payload):
    hexPacket = " ".join(["%02x" % ord(x) for x in payload])
    print packetIndex, timestamp, hexPacket

def runLoop(sockListen):
    if sockListen != None:
        sockSend, address = sockListen.accept()
        print "***", address
    else: sockSend = None

    headerSpec = "<BIQB"
    headerSize = struct.calcsize(headerSpec)

    startTime = time.time()
    while True:
        rawPacket = s.recv(100000)
        packet = rawPacket
        if udpSendAddress != None:
            sockUdpSend.sendto(rawPacket, udpSendAddress)

        # see SWRU187F
        (flags, packetIndex, timestamp, size 
         ) = struct.unpack(headerSpec, packet[0:headerSize])
        packet = packet[headerSize:]
        if flags&1 != 0:
            # length includes FCS ; adjust
            size = size - 2
        payload = packet[0:size]

        if sockSend != None:
            sockSend.send(struct.pack("!I", len(rawPacket)))
            sockSend.send(rawPacket)
        displayPacket(packetIndex, timestamp, payload)


Port = 5000

udpSendAddress = None
#udpSendAddress = ("192.168.1.100", 5000)
udpSendAddress = ("10.0.2.2", Port)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', Port))

if udpSendAddress != None:
    sockUdpSend = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

forward = False

if forward:
    sockListen = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockListen.bind(('', 5555))
    sockListen.listen(1)
else: sockListen = None

print "*** Forwarding (UDP) packet received on port %s to address %s port %s" \
    % (Port, udpSendAddress[0], udpSendAddress[1])

while True:
    if forward:
        try: runLoop(sockListen)
        except: print "restart"
    else: runLoop(None)
    runLoop(None) 

#---------------------------------------------------------------------------
