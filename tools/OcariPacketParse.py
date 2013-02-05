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

import warnings

import OperaPacketParse
from OperaPacketParse import popStruct, ra, ra2
import struct

#---------------------------------------------------------------------------

class OcariPacket:
    def __init__(self):
        pass

    def hexdump(self):
        print "(XXX:not implemented yet)"        

    def summary(self):
        return "(XXX:not implemented yet)"

#---------------------------------------------------------------------------

# http://staff.ustc.edu.cn/~ustcsse/papers/SR10.ZigBee.pdf


def popBit(flag, nbBit):
    value = flag & ((1<<nbBit)-1)
    return value, (flag>>nbBit)

ZigbeeFrameType = {
    0: "data", # ZBS - p308 l36
    1: "nwk", # ZBS - p308 l36
    2: "<XXX:not implemented>",
    3: "<XXX:not implemented>"
 }

def parseZigbee(packet):
    # ZigBee Specification - Document 053474r17 - 2007
    # p 307
    (frameControl,), packet = popStruct("H", packet)
    data = frameControl
    frameType, data = popBit(data, 2)
    protocolVersion, data = popBit(data, 4)
    discoverRoute, data = popBit(data, 2)
    multicast, data = popBit(data, 1)
    security, data = popBit(data, 1)
    sourceRoute, data = popBit(data, 1)
    dstIEEEAddress, data = popBit(data,1)
    srcIEEEAddress, data = popBit(data, 1)
    reserved, data = popBit(data, 3)
    assert data == 0

    if ZigbeeFrameType[frameType] != "nwk":
        return None

    (dstAddress,), packet = popStruct("2s", packet)
    (srcAddress,), packet = popStruct("2s", packet)
    (radius,), packet = popStruct("B", packet)
    (seqNum,), packet = popStruct("B", packet)

    if srcIEEEAddress: (srcLongAddress,), packet = popStruct("8s", packet)
    else: srcLongAddress = None

    if dstIEEEAddress: (dstLongAddress,), packet = popStruct("8s", packet)
    else: dstLongAddress = None

    r = { "frame-control": 
          { "frame-type": ZigbeeFrameType[frameType],
            "version": protocolVersion,
            "discover-route": discoverRoute, # see p309 l10-13
            "multicast": multicast,
            "security": security,
            "source-route": sourceRoute,
            "dst-ieee": dstIEEEAddress,
            "src-ieee": srcIEEEAddress },
          "src": srcAddress,
          "dst": dstAddress,
          "radius": radius,
          "seq-num": seqNum,
          "payload": packet
          }

    if srcLongAddress != None: 
        r["src-long"] = srcLongAddress
    if dstLongAddress != None:
        r["dst-long"] = dstLongAddress

    return r

MacCommandType = {
    0x01: "association-request",
    0x02: "association-response",
    0x03: "disassociation-notification",
    0x04: "data-request",
    0x05: "pan-id-conflict-notification",
    0x06: "orphan-notification",
    0x07: "beacon-request",
    0x08: "coordinator-realignment",
    0x09: "gts-request"
}

AssociationStatusTable = {
    0x00: "successful",
    0x01: "pan-at-capacity",
    0x02: "pan-access-denied"
}

def parseMacCommand(payload):
    # 15.4 p112
    #return " ".join(["%02x" % ord(x) for x in payload])
    (command,), payload = popStruct("B", payload)
    command = MacCommandType.get(command, command)
    r = { "command": command }
    if command == "association-request":
        pass
    elif command == "association-response":
        (address, statusField), payload = popStruct("2sB", payload)
        r["address"] = address
        r["status"] = AssociationStatusTable.get(statusField, statusField)
    else:
        r["payload"] = " ".join(["%02x" % ord(x) for x in payload])
    return r

def parseOcariBeacon(payload):
    # ZigBee Specification - Document 053474r17 - 2007
    # p 414
    #return " ".join(["%02x" % ord(x) for x in payload])

    (protocolID, stackInfo, deviceInfo), payload = popStruct("BBB", payload)

    stackProfile, stackInfo = popBit(stackInfo, 4)
    nwkProtocolVersion, zero = popBit(stackInfo, 4)
    assert zero == 0
    
    reserved, deviceInfo = popBit(deviceInfo, 2)
    routerCapacity, deviceInfo = popBit(deviceInfo, 1)
    deviceDepth, deviceInfo = popBit(deviceInfo, 4)
    endDeviceCapacity, zero = popBit(deviceInfo, 1)
    assert zero == 0

    (extPanId,), payload = popStruct("8s", payload)

    # -- This is Zigbee
    #(txOffset1, txOffset2, txOffset3), payload = popStruct("BBB", payload)
    #(nwkUpdateId,), payload = popStruct("B", payload)
    # txOffset = (txOffset3<<16) + (txOffset2<<8) + txOffset1

    # -- This is Ocari
    (nbCoord, cycleSeqNum, globalCycleDuration,
     activityDuration
     ), payload = popStruct("<BBIH", payload)
    coordAddressList = []

    for i in range(nbCoord):
        (coordAddress,),payload = popStruct("2s", payload)
        coordAddressList.append(coordAddress)
    #(activityBitmap,), payload = popStruct("I", payload)
    activityBitmap = "<XXX:is field present?>"
       
    #warnings.warn("note: not checking properly if there is a color seq.")
    # XXX
    if len(payload) > 0:
        (colorSeq,),payload = popStruct("B", payload)
    else: colorSeq = None
    if len(payload) > 0:
        (coloredSlotDuration,),payload = popStruct("B", payload)
    else: coloredSlotDuration = None
    
    r = {
        "protocol-id": protocolID,
        "stack-profile": stackProfile,
        "protocol-version": nwkProtocolVersion,
        "router-capacity": routerCapacity,
        "device-depth": deviceDepth,
        "end-device-capacity": endDeviceCapacity,
        "pan-id": extPanId,
        #"txOffset": txOffset,
        #"nwkUpdateId": nwkUpdateId,
        "nb-coord": nbCoord,
        "cycle-seq-num": cycleSeqNum,
        "global-cycle-duration": globalCycleDuration,
        "activity-duration": activityDuration,
        "coord-address-list": coordAddressList,

        "activity-bitmap": activityBitmap,
        "color-sequence": colorSeq,
        "colored-slot-duration": coloredSlotDuration,
            
        "unparsed":" ".join(["%02x" % ord(x) for x in payload]) # XXX!!
        }

    return r
    

Dot15_4FrameType = {
    0: "beacon", # IEEE 802.15.4 p 112
    1: "data",
    2: "ack",
    3: "MAC-command",
    4: "reserved",
    5: "reserved",
    6: "reserved",
    7: "reserved"
 }

Dot15_4AddressingMode = {
    0: "empty", # IEEE 802.15.4 p 113
    1: "reserved",
    2: "short",
    3: "long"
}


def parseDot15_4(packet):
    # IEEE Std 802.15.4-2003 p112

    (frameControl,), packet = popStruct("H", packet)
    data = frameControl
    frameType, data = popBit(data, 3)
    security, data = popBit(data, 1)
    pending, data = popBit(data, 1)
    ackRequest, data = popBit(data, 1) 
    intraPan, data = popBit(data, 1)
    reserved, data = popBit(data, 3)
    dstAddressMode, data = popBit(data, 2) 
    reserved2, data = popBit(data,2)
    srcAddressMode, data = popBit(data, 2)

    (seqNum,), packet = popStruct("B", packet)
    if dstAddressMode != 0: 
        (dstPanId,), packet = popStruct("H", packet)
        if Dot15_4AddressingMode[dstAddressMode] == "short":
            (dstAddress,), packet = popStruct("2s", packet)
        else: (dstAddress,), packet = popStruct("8s", packet)
        dst = (dstPanId, dstAddress)
    else: dst = None

    if srcAddressMode != 0:
        if not intraPan:
            (srcPanId,), packet = popStruct("H", packet)
        else: srcPanId = None
        if Dot15_4AddressingMode[srcAddressMode] == "short":
            (srcAddress,), packet = popStruct("2s", packet)
        else: (srcAddress,), packet = popStruct("8s", packet)
        src = (srcPanId, srcAddress)
    else: src = None

    # for OCARI the following is added:
    # (see: 'DR5_Specification_des_interfaces_V11' p28, but modified
    #  coloring-mode is now put at priority)
    # - coloring mode
    # - constrained/priority/color ind.
    constrained, reserved = popBit(reserved, 1)
    coloringMode, reserved = popBit(reserved, 1)
    colorIndication, reserved = popBit(reserved, 1)
    
    r = { "frame-control":
              { "frame-type": Dot15_4FrameType[frameType],
                "security": security,
                "pending": pending,
                "ack-request": ackRequest,
                "intra-pan": intraPan,
                "dst-address-mode": Dot15_4AddressingMode[dstAddressMode],
                "src-address-mode": Dot15_4AddressingMode[srcAddressMode],
                "seq-num": seqNum,
                "ocari": {
                   "coloring-mode": coloringMode,
                   "constrained": constrained,
                   "color-indication": (colorIndication)
                   }
                },
          "dst": dst,
          "src": src,
          "payload": packet
          }

    frameType = r["frame-control"]["frame-type"]
    if frameType == "data":
        r["zigbee"] = parseZigbee(packet)
    elif frameType == "beacon":
        r["beacon"] = parseBeacon(packet)
    elif frameType == "MAC-command":
        r["MAC-command"] = parseMacCommand(packet)

    return r

def parseBeacon(packet):
    # IEEE Std 802.15.4-2003 p116-117

    # Superframe Spec
    (superFrameSpecField,), packet = popStruct("H", packet)
    data = superFrameSpecField
    bo, data = popBit(data, 4)
    superFrameOrder, data = popBit(data,4)
    finalCapSlot, data = popBit(data, 4)
    battery, data = popBit(data, 1)
    reserved,data = popBit(data, 1)
    cpan, data = popBit(data, 1)
    associationPermit, data = popBit(data, 1)
    assert data == 0

    superFrameSpec = {
        "cpan": cpan,
        "association-permit": associationPermit
        # ... XXX this can be filled
        }

    # GTS
    (gtsSpec,), packet = popStruct("B", packet)
    gtsDescriptorCount, gtsSpec = popBit(gtsSpec, 3)
    reserved, gtsSpec = popBit(gtsSpec, 4)
    gtsPermit, gtsSpec = popBit(gtsSpec, 1)
    assert gtsSpec == 0

    gtsList = []
    if gtsDescriptorCount > 0:
        (gtsDirections,), packet = popStruct("B", packet)
        for i in range(gtsDescriptorCount):
            (shortAddress, gtsInfo), packet = popStruct("2sB", packet)
            gtsDesc = { "address": shortAddress, 
                        "start-slot": gtsInfo & 0x0f,
                        "nb-slot": (gtsInfo & 0xff) >> 4 }
            gtsList.append(gtsDesc)
    else: gtsDirections = None

    # Pending addresses
    (pendingAddressSpec,), packet = popStruct("B", packet)
    shortAddrPending, spec = popBit(pendingAddressSpec, 3)
    reserved2, spec = popBit(spec, 1)
    extAddrPending, spec = popBit(spec, 3)
    reserved3, spec = popBit(spec, 1)
    assert spec == 0

    pendingAddressList = []
    for i in range(shortAddrPending):
        shortAddress, packet = popStruct("2s", packet)
        pendingAddressList.append(shortAddress)
    for i in range(extAddrPending):
        extAddress, packet = popStruct("8s", packet)
        pendingAddressList.append(extAddress)

    # Beacon payload
    payload = packet

    # Return valu
    gts = {
        "count": gtsDescriptorCount,
        "permit": gtsPermit, 
        #"reserved": reserved,
        "gts-list": gtsList
        }
    
    # for OCARI the following is added:
    # (see: 'DR5_Specification_des_interfaces_V11' p31-32)

    r = { "superframe": superFrameSpec,
          "gts": gts,
          "pending-list": pendingAddressList,
          "payload": parseOcariBeacon(payload)
          #"unparsed": packet,
          #"beacon": " ".join(["%02x" % ord(x) for x in payload])
          }
    #print r

    return r

parsePacket = parseDot15_4

#---------------------------------------------------------------------------

def reprAddress(address):
    if len(address) == 2:
        address = "%02x" % struct.unpack("H", address)[0]
    else: address = "%08x" % struct.unpack("Q", address)[0]
    return address


class FrameAnalysis:
    def __init__(self, notifyNewCycleFunction = None):
        self.notifyNewCycleFunction = notifyNewCycleFunction
        self.repeaterList = []

    def addPacket(self, timestamp, packetInfo):
        if "beacon" in packetInfo:
            self.addBeacon(timestamp, packetInfo)

    def addBeacon(self, timestamp, packetInfo):
        superframe = packetInfo["beacon"]["superframe"]

        if superframe["cpan"]:
            if self.notifyNewCycleFunction != None:
                self.notifyNewCycleFunction(timestamp, packetInfo)

            self.referenceBeacon = packetInfo
            self.repeaterList = []

        strSrc = reprAddress(packetInfo["src"][1])
        self.repeaterList.append(strSrc)



#---------------------------------------------------------------------------
