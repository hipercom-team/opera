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

import struct

#---------------------------------------------------------------------------

MsgTypeHello = ord("H")
MsgTypeSTC = ord("S")
MsgTypeTreeStatus = ord("T")
MsgTypeColor = ord("C")

AddressSize = 2

def ra(x): 
    if SimulMode: return "%s"%x
    else: return "%x" % x

def ra2(x): 
    if SimulMode: return "@%s"%x
    else: return "@%x" % x

#---------------------------------------------------------------------------

def popStruct(spec, data):
    size = struct.calcsize(spec)
    return struct.unpack(spec, data[:size]), data[size:]

def popAddress(data):
    if SimulMode:
        return (struct.unpack("!H", data[:AddressSize])[0],
                # XXX: if AddressSize!=2
                data[AddressSize:])
    else:
        return (struct.unpack("H", data[:AddressSize])[0],
                # XXX: if AddressSize!=2
                data[AddressSize:])

#---------------------------------------------------------------------------

NeighState = { 
    0: "none", 
    1: "asym", 
    2: "sym",
    ord('S'): "sys-info",
    ord('M'): "sys-message",
    ord("L"): "stat"
}

SimulMode = False # XXX: hack to indicate whether packets are from python simuls

def parseHelloMessage(packet):
    result = {'type': "Hello"}

    # Header
    data = packet
    #(messageType, messageSize), data = popStruct("BB", data)
    #if messageType != MsgTypeHello:
    #    raise RuntimeError("Unknown message type", messageType)
    address, data = popAddress(data)
    (seqNum, vtime, energy), data = popStruct("!HBB", data)
    result.update({ "address": address, "seqNum": seqNum,
                    "vtime": vtime, "energyClass": energy })

    if SimulMode:
        (result["estimateNbTwoHop"],), data = popStruct("B", data)

    linkSetList = []
    neighAddrList = []
    while len(data) > 0:
        (linkCode, linkMessageSize), data = popStruct("BB", data)
        linkData = data[:linkMessageSize]
        data = data[linkMessageSize:]

        if linkCode not in NeighState:
            linkSetList.append( (linkCode, linkData))
            continue

        linkCodeStr = NeighState.get(linkCode, None)
        if linkCodeStr == "sym" or linkCodeStr == "asym":
            linkList =[]
            while len(linkData) > 0:
                address, linkData = popAddress(linkData)
                linkList.append(address)
        else: linkList = linkData

        if linkCodeStr == "sym":
            linkSetList.append( ("sym", linkList) )
            neighAddrList = neighAddrList + linkList

        elif linkCodeStr == "asym":
            linkSetList.append( ("asym", linkList) )
            neighAddrList = neighAddrList + linkList

        elif linkCodeStr == "sys-info":
            #s += " sysInfo="
            ##+ "".join(
            ##["%02x" % ord(x) for x in linkList])
            sysInfo,stability,colorInfo = struct.unpack("!HBB", linkList[0:4])
            #s += "%02x:" % sysInfo
            result["sys-info"] =  [SystemInfo[x] for x in range(8)
                          if (x in SystemInfo) and ((1<<x) & sysInfo) != 0]
            if colorInfo != 0:
                result["color-info"] = {
                    "color": (colorInfo&0xf)-1,
                    "nb-color": (colorInfo >> 4)
                    }
        elif linkCodeStr == "sys-message":
            fileName = linkList[2:]
            pos = fileName.find('\x00')
            if pos >= 0:
                fileName = (fileName[pos+1:] + fileName[:pos]
                            ).replace('\x00', "")
            result["sys-message"] = (fileName,
                                     struct.unpack("!H",linkList[:2])[0])
        elif linkCodeStr == "stat":
            assert struct.calcsize("H")*len(neighAddrList) == len(linkList)
            neighStatTable = {}
            for i,neighAddr in enumerate(neighAddrList):
                stat = struct.unpack("!H", linkList[2*i:2*i+2])[0]
                #neighStat = (ra(neighAddrList[i]) + ":"
                #             + ",".join(
                neighStat = [((stat >> (4*j)) & 0xf) for j in range(4)]
                #neighStatList.append(neighStat)
                neighStatTable[neighAddrList[i]] = neighStat
            #s += " stat=" + "/".join(neighStatList)
            result["stat"] = neighStatTable

        else: linkSetList.append( (linkCode, linkList) )
      
    result["linkInfo"] = linkSetList
    return result

SystemInfo = {
    8: "BroadcastOverflow",
    7: "ColoringModeRequestCalled",
    6: "MaxColorRequestCalled",
    5: "MaxColorResponseCalled",
    4: "ColoringModeIndicationCalled",
    3: "MaxColorIndicationCalled",
    2: "coloring-mode-on",
    1: "hasWarning",
    0: "hasError"
}

def reprHelloMessage(packetAsStruct, verbosity = 0):
    p = packetAsStruct
    s = "[Hello(%s):" % ra(p["address"])
    if (verbosity >= 0): 
        s += " seq=%s"% p["seqNum"]
    if (verbosity >= 2): 
        s += " vtime=%s energy=%s" % (p["vtime"], p["energyClass"])
    else: s += " energyClass=%s" % (p["energyClass"],)

    symList = []
    asymList = []
    for linkCode, linkList in p["linkInfo"]:
        if linkCode == "sym": symList = linkList
        elif linkCode == "asym": asymList = linkList
    neighAddrList = symList + asymList

    for linkCode, linkList in p["linkInfo"]:
        if linkCode in ["sym", "asym"]:
            s += " %s" % linkCode + "=" + ",".join([ra(x) for x in linkList])

    if verbosity >= 1 and "color-info" in p:
        if p["color-info"]["nb-color"] != 0:
            s += " color=%s/%s" % (p["color-info"]["color"],
                                     p["color-info"]["nb-color"])
        else: s += " color=%s" % p["color-info"]["color"]

    if verbosity >= 1 and "sys-info" in p:
        s += " sysInfo=" + ",".join(p["sys-info"])

    if verbosity >= 1 and "sys-message" in p:
        fileName, line = p["sys-message"]
        s += " warn=%s:%d" % (fileName, line)
        
    if verbosity >= 1 and "stat" in p:
        neighStatList = []
        for i,neighAddr in enumerate(neighAddrList):
            if neighAddr in p["stat"]:
                neighStat = (ra(neighAddr) + ":"
                             +",".join(["%s"%x for x in p["stat"][neighAddr]]))
            neighStatList.append(neighStat)
        s += " stat=" + "/".join(neighStatList)        
                          

    s += "]"
    return s

#---------------------------------------------------------------------------

#define EOSTC_FLAG_COLORED_BIT            7
#define EOSTC_FLAG_STABLE_BIT             6
#define EOSTC_FLAG_BECAME_UNSTABLE_BIT    5
#define EOSTC_FLAG_INCONSISTENT_STABILITY 4
#define EOSTC_FLAG_TREE_CHANGE            2
#define EOSTC_FLAG_COLOR_CONFLICT         1
#define EOSTC_FLAG_NEW_NEIGHBOR           0

STCFlag = {
    7: "to-color", # tree
    6: "stable",
    5: "unstable",
    4: "inconsistent-stability",
    2: "tree-change",
    1: "conflict", # XXX: not used
    0: "new-neigh"
}

def parseSTCMessage(packet):
    result = {'type': "STC"}
    result["sender"], data = popAddress(packet)
    (result["seqNum"],), data = popStruct("!H", data)
    #popAddress(data)
    result["root"], data = popAddress(data)
    (result["cost"],), data = popStruct("!H", data)
    result["parent"], data = popAddress(data)
    (result["vtime"], 
     result["ttl"], 
     flag, 
     result["treeSeqNum"]), data = popStruct("!BBBH", data)
    
    result["flag"] = [name for i, name in STCFlag.iteritems()
                      if ((1<<i) & flag) != 0]
    result["flagValue"] = ["%s"%flag]

    if len(data) > 0:
        result["extraData"] = data
    return result

def reprSTCMessage(packetAsStruct, verbosity = 0):
    p = packetAsStruct
    s = "[STC("+ra(p["sender"])+")"
    if p["sender"] != p["parent"]:
        s += "->" + ra(p["parent"])
    if p["parent"] != p["root"]:
        s += "-.->" + ra(p["root"])
    s += " #%s.%s" % (p["treeSeqNum"],p["seqNum"])
    if len(p["flag"]) > 0:
        s += " " + ",".join(p["flag"])
    s += " d=%s" % (p["cost"])
    if (verbosity >= 2):
        s += " vtime=%s ttl=%s" % (p["vtime"], p["ttl"])
    if "extraData" in p:
        s += " extra=" + repr(p["extraData"])
    s += "]"
    return s

#---------------------------------------------------------------------------

def parseTreeStatusMessage(packet):
    result = {'type': "TreeStatus"}
    result["sender"], data = popAddress(packet)
    (result["seqNum"],), data = popStruct("!H", data)
    #popAddress(data)
    result["root"], data = popAddress(data)
    (result["treeSeqNum"], 
     result["nbChild"],
     flag), data = popStruct("!HHB", data)

    result["flag"] = [name for i, name in STCFlag.iteritems()
                      if ((1<<i) & flag) != 0]
    result["flagValue"] = ["%s"%flag]

    if len(data) > 0:
        result["extraData"] = data
    return result

def reprTreeStatusMessage(packetAsStruct, verbosity = 0):
    p = packetAsStruct
    s = "[TreeStatus("+ra(p["sender"])+")"
    if p["sender"] != p["root"]:
        s += "-.->" + ra(p["root"])
    s += " #%s.%s" % (p["treeSeqNum"],p["seqNum"])
    if len(p["flag"]) > 0:
        s += " " + ",".join(p["flag"])
    s += " child=%s" % (p["nbChild"])
    #if (verbosity >= 2):
    #    s += " vtime=%s ttl=%s" % (p["vtime"], p["ttl"])
    if "extraData" in p:
        s += " extra=" + repr(p["extraData"])
    s += "]"

    return s

#---------------------------------------------------------------------------


def bitmapToList(data):
    result = []
    for i in range(len(data)):
        for j in range(8):
            if ord(data[i])&(1<<j) != 0:
                result.append(i*8+j)
    return result

def parseSerenaPacket(packet):

    
    result = { "type": messageType, 
               "address": fromRawAddress(address), 
               "color": color, 
               "priority": myPriority,
               "max2Prio1": max2Prio1, 
               "max2Prio2": max2Prio2, 
               }
    return result

PriorityNone = 0xff
ColorNone = 0xff

def popPriority(data):
    if SimulMode: (priority,), data = popStruct("!I", data)
    else: (priority,), data = popStruct("!B", data)
    return priority, data

def parseColorMessage(packet):
    result = {'type': "Color"}
    result["sender"], data = popAddress(packet)
    result["root"], data = popAddress(data)
    (result["treeSeqNum"], 
     result["maxColor"],
     result["color"]), data = popStruct("!HBB", data)
    #result["priority"], nbMax2Prio1
    result["priority"], data = popPriority(data)

    if result["maxColor"] == 0:
        result["maxColor"] = None
    if result["color"] == ColorNone:
        result["color"] = None
    if result["priority"] == PriorityNone:
        result["priority"] = None

    (nbMax2Prio1,), data = popStruct("B", data)
    max2Prio1 = []
    for i in range(nbMax2Prio1):
        priority, data = popPriority(data)
        address, data = popAddress(data)
        max2Prio1.append((priority, address))

    (nbMax2Prio2,), data = popStruct("B", data)
    max2Prio2 = []
    for i in range(nbMax2Prio2):
        priority, data = popPriority(data)
        address, data = popAddress(data)
        max2Prio2.append((priority, address))

    (sizeBitmap1,), data = popStruct("B", data)
    (bitmap1,), data = popStruct("%ds" % sizeBitmap1, data)

    (sizeBitmap2,), data = popStruct("B", data)
    (bitmap2,), data = popStruct("%ds" % sizeBitmap2, data)

    if len(data) > 0:
        (seqNum,), data = popStruct("!H", data)
    else: seqNum = None

    if len(data) > 0:
        result["extraData"] = data

    result["maxPrio1"] = max2Prio1
    result["maxPrio2"] = max2Prio2

    result["colorBitmap1"] = bitmapToList(bitmap1)
    result["colorBitmap2"] = bitmapToList(bitmap2)

    result["seqNum"] = seqNum

    return result


def reprColorMessage(packetAsStruct, verbosity = 0):
    p = packetAsStruct
    s = "[Color(%s)" % ra(p["sender"])

    s += " seq=%s"% p["seqNum"]
    if verbosity >= 1:
        s += " " + ra2(p["root"]) +":%s" % (p["treeSeqNum"])

    if p["color"] != None: 
        s += " color=%s" % p["color"]
    if p["maxColor"] != None: 
        s += "/%s" % p["maxColor"] 
    #if verbosity >= 1 and p["color"] == None:

    s += " prio=%s" % p["priority"]
    if len(p["maxPrio1"]) > 0:
        s += " >" + ",".join(["%s.%s" % (prio, ra2(addr))
                              for (prio,addr) in p["maxPrio1"]])
    if len(p["maxPrio2"]) > 0:
        s += " >>" + ",".join(["%s.%s" % (prio, ra2(addr))
                               for (prio,addr) in p["maxPrio2"]])

    if len(p["colorBitmap1"]) > 0:
        s += " c=" + ",".join(["%x" % x for x in p["colorBitmap1"]])
    if len(p["colorBitmap2"]) > 0:
        if len(p["colorBitmap1"]) > 0: s += ";"
        else: s += " c=;"
        s += ",".join(["%x" % x for x in p["colorBitmap2"]])
        
    if "extraData" in p:
        s += " extra=" + repr(p["extraData"])

    s += "]"
    return s

#---------------------------------------------------------------------------

def parsePacket(packet):
    data = packet
    if len(packet) < 2: return { "type": "packet-too-short" }
    (messageType,messageSize), data = popStruct("BB", data)
    if messageType == MsgTypeHello:
        return parseHelloMessage(data)
    elif messageType == MsgTypeSTC:
        return parseSTCMessage(data)
    elif messageType == MsgTypeTreeStatus:
        return parseTreeStatusMessage(data)
    elif messageType == MsgTypeColor:
        return parseColorMessage(data)
    else: return {'type': messageType, 'content': data }

#---------------------------------------------------------------------------

def reprPacket(packetAsStruct, verbosity = 0):
    if packetAsStruct.get("type") == "Hello":
        return reprHelloMessage(packetAsStruct, verbosity)
    elif packetAsStruct.get("type") == "STC":
        return reprSTCMessage(packetAsStruct, verbosity)
    elif packetAsStruct.get("type") == "TreeStatus":
        return reprTreeStatusMessage(packetAsStruct, verbosity)
    elif packetAsStruct.get("type") == "Color":
        return reprColorMessage(packetAsStruct, verbosity)
    else: return repr(packetAsStruct)

#---------------------------------------------------------------------------

if __name__ == "__main__":
    SimulMode = True
    data = [0x48, 0xd, 0x27, 0x12, 0x0, 0x17, 0x0, 0x2, 0x4, 0x2, 0x4, 0x27, 0x10, 0x27, 0x16]
    data2 = "".join([chr(x) for x in data])
    print parsePacket(data2)

#---------------------------------------------------------------------------
