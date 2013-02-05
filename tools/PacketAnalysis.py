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

import socket, sys, struct, time, os, optparse, traceback
import select, tempfile, warnings, pprint

sys.path.append("../../pkg-python/HipSens/Core")
sys.path.append("Lib")

#import pcapy # apt-get install python-pcapy
## doc: http://oss.coresecurity.com/pcapy/doc/pt01.html

#--------------------------------------------------

try:
    import color_console as cons
except:
    if sys.platform == "win32":
        warnings.warn("'color_console' could not be imported.")
    cons = None

if cons != None:
    oldWinConsoleColor = cons.get_text_attr()
    cons.set_text_attr(cons.BACKGROUND_GREY | cons.BACKGROUND_INTENSITY
                       | cons.FOREGROUND_BLACK)
    os.system("cls")

#--------------------------------------------------

try:
    sys.path.insert(0, "../../support/build/scapy-com")
    DONT.LOAD.SCAPY # even when available
    import scapy
    from scapy.layers.dot15d4 import *
    # hg clone http://hg.secdev.org/scapy-com, etc...
except: 
    print "NOTE: not using scapy"
    #print "***WARNING***: the parser assumes that the panID is :",
    #print "".join(["%02x" % ord(x) for x in reversed(list(panId))])
    scapy = None



import OperaPacketParse
from OperaPacketParse import popStruct
import OcariPacketParse

#---------------------------------------------------------------------------

Port = 5000
TableSendPort = 5001
TableSendPacketSize = 16

# defined in   -  include/nwk.h:
NWK_CDE_OPERA = 0x0B

#---------------------------------------------------------------------------


if sys.platform == "win32": 
    stateDir = tempfile.gettempdir()
    print "platform win32: storing state in directory:", stateDir
else: 
    stateDir = "/dev/shm"
    print "platform linux: storing state in directory:", stateDir

#---------------------------------------------------------------------------

def popStruct(spec, data):
    specSize = struct.calcsize(spec)
    return struct.unpack(spec, data[:specSize]), data[specSize:]

def toHex(payload):
    return " ".join(["%02x" % ord(x) for x in payload])

def displayPacket(packetIndex, timestamp, payload):
    hexPacket = toHex(payload)
    #print packetIndex, timestamp, hexPacket
    print "%.6f"% timestamp, " ", hexPacket

    #(frameControl, seqNum), data = popStruct("!HB", payload)
    #print frameControl, seqNum

#---------------------------------------------------------------------------

# http://www.tcpdump.org/linktypes.html
LINKTYPE_IEEE802_15_4 = 195
LINKTYPE_IEEE802_15_4_NOFCS = 230
MaxPacketSize = 65536

# http://wiki.wireshark.org/Development/LibpcapFileFormat
PcapHeader = struct.pack(
    "!IHHIIII",  
    0xa1b2c3d4, # Magic
    2, # Major version
    4, # Minor version
    0, # thiszone
    0, # accuracy of timestamps
    MaxPacketSize, # max captured packet size
    LINKTYPE_IEEE802_15_4 # data link type
    )

def pcapFormatPacket(clock, packet):
    actualPacketSize = len(packet)
    packetSize = min(actualPacketSize, MaxPacketSize)
    result = struct.pack("!IIII", 
                         int(clock), # seconds
                         (clock-int(clock))* 1000000, # microseconds
                         packetSize,
                         actualPacketSize)
    result += packet[:packetSize]
    return result

# see also 
# http://wiki.wireshark.org/Python

#---------------------------------------------------------------------------

def selectColorWinConsole(name):
    if name == "green": color = cons.FOREGROUND_GREEN
    elif name == "blue": color = cons.FOREGROUND_BLUE
    elif name == "red": color = cons.FOREGROUND_RED
    elif name == "high": color=  cons.FOREGROUND_BLUE|cons.FOREGROUND_INTENSITY
    elif name == "normal": color = cons.FOREGROUND_BLACK
    else: raise ValueError("Unknown 'color' name", name)
    fullColor = cons.BACKGROUND_GREY | cons.BACKGROUND_INTENSITY | color
    cons.set_text_attr(fullColor)

def selectColorAnsiOrHtml(name):
    if name == "green": sys.stdout.write(DCgreen)
    elif name == "blue": sys.stdout.write(DCblue)
    elif name == "red": sys.stdout.write(DCred)
    elif name == "high": sys.stdout.write(DChigh)
    elif name == "normal": sys.stdout.write(DCnorm)
    else: raise ValueError("Unknown 'color' name", name)

def selectColor(name):
    if cons != None: selectColorWinConsole(name)
    else: selectColorAnsiOrHtml(name)

#---------------------------------------------------------------------------

operaVolume = 0

def showOperaPacket(rawPacket, infoTable):
    global operaVolume
    try:
        structPacket = OperaPacketParse.parsePacket(rawPacket)
        strPacket = OperaPacketParse.reprPacket(structPacket, 1)
    except: 
        print "PARSE ERROR************************"
        traceback.print_exc()
        #sys.exit(1)
        print "   ", toHex(rawPacket)
        return

    def ensureNodeAddress(addr):
        if addr not in infoTable["nodeTable"]:
            infoTable["nodeTable"][addr] = { 
                "tree": {}, "treeStatus": {}  }
        return infoTable["nodeTable"][addr]

    #print structPacket
    structPacket["timestamp"] = infoTable["timestamp"]
    packetType = structPacket.get("type", None)
    if "nodeTable" not in infoTable:
        infoTable["nodeTable"] = {}
    if packetType == "Hello":
        address = structPacket["address"]
        nodeInfo = ensureNodeAddress(address)
        nodeInfo["lastHello"] = structPacket
    elif packetType == "STC":
        sender = structPacket["sender"]
        nodeInfo = ensureNodeAddress(sender)
        nodeInfo["tree"][structPacket["root"]] = structPacket
    elif packetType == "TreeStatus":
        sender = structPacket["sender"]
        nodeInfo = ensureNodeAddress(sender)
        nodeInfo["treeStatus"][structPacket["root"]] = structPacket
    elif packetType == "Color":
        sender = structPacket["sender"]
        nodeInfo = ensureNodeAddress(sender)
        nodeInfo["lastColor"] = structPacket
    #print structPacket

    #print "%.6f" %actualTime,
    if strPacket.startswith("[Hello"): selectColor("green")
    elif strPacket.startswith("[STC"): selectColor("blue")
    elif strPacket.startswith("[Color"): selectColor("red")
    elif strPacket.startswith("[TreeS"): selectColor("high")
    print strPacket
    selectColor("normal")
    operaVolume += len(rawPacket)
    #print operaVolume

# CF 19eme octet de la trame. (12eme)


lastWasBeacon = True
def showBeaconPacket(timestamp, packetInfo):
    global lastWasBeacon
    beacon = packetInfo["beacon"]["payload"]

    if packetInfo["beacon"]["superframe"]["cpan"]:
        print "-"*70
        selectColor("high")
    else: selectColor("normal")    
    sys.stdout.write("%.6f" % timestamp)


    sys.stdout.write(" beacon")
    if packetInfo["src"] != None:
        panId, address = packetInfo["src"]
        sys.stdout.write(OcariPacketParse.reprAddress(address) 
                         + "(%04x)" % panId)
        coordStr = ",".join([OcariPacketParse.reprAddress(address)
                             for address in beacon["coord-address-list"]])
        sys.stdout.write(" #%s coord=%s cycle=%sms" %
                         (beacon["cycle-seq-num"],
                          coordStr, #beacon["nb-coord"],
                          beacon["global-cycle-duration"]*0.32
                          ))
    selectColor("normal")
    if packetInfo["frame-control"]["ocari"]["coloring-mode"]:
        selectColor("high")
    else: selectColor("normal")    
    if packetInfo["beacon"]["superframe"]["cpan"]:
        sys.stdout.write("\n     -- ")
        if packetInfo["frame-control"]["ocari"]["coloring-mode"]:
            sys.stdout.write("coloring ")
        sys.stdout.write("color-indic=%d"
                         % packetInfo["frame-control"]["ocari"]["color-indication"])
    #if packetInfo["frame-control"]["ocari"]["color-indication"]:
    #    sys.stdout.write(" use-color")
    sys.stdout.write(" " + packetInfo["beacon"]["payload"]["unparsed"]
                     + "\n")
    if packetInfo["beacon"]["superframe"]["cpan"]:
        pprint.pprint(packetInfo)
    lastWasBeacon = True
    #sys.stdout.write("\n")

def eventNewSuperframe(timestamp, packetInfo):
    global frameAnalysis
    sys.stdout.write(" (prev:" + "/".join(frameAnalysis.repeaterList)+") -- ")
    showBeaconPacket(timestamp, packetInfo)


def showZigbeePacket(packetIndex, timestamp, packet, infoTable, packetInfo):
    global lastWasBeacon
    if lastWasBeacon: print
    lastWasBeacon = False
    if packetInfo["dst"] == None: return
    if packetInfo["dst"][1] != "\xff\xff": return
    zigbeePayload = packetInfo["zigbee"]
    #print zigbeePayload
    if (zigbeePayload == None 
        or zigbeePayload["frame-control"]["frame-type"] != "nwk"):
        return
    data = zigbeePayload["payload"]
    if len(data) < 3: return
    if ord(data[0]) != NWK_CDE_OPERA: return
    packetLen = ord(data[1])

    #print "%.6f"% timestamp, 
    #displayPacket(packetIndex, timestamp, packet)
    sys.stdout.write("%.6f " % timestamp)
    if "src-long" in zigbeePayload and zigbeePayload["src-long"] != None:
        srcLong = "%016x" % struct.unpack("Q",zigbeePayload["src-long"])
        srcLong = ".."+srcLong[-6:]
        sys.stdout.write(srcLong)
    srcShort = struct.unpack("H",zigbeePayload["src"])[0]
    dstShort = struct.unpack("H",zigbeePayload["dst"])[0]
    sys.stdout.write("(%04x->%04x)" % (srcShort, dstShort))
    sys.stdout.write(" -- ")
    showOperaPacket(data[2:packetLen+2], infoTable)
    print

#def showBeaconPacket(packetIndex, timestamp, packet, infoTable, packetInfo):
#    if packetInfo["beacon"] == None: return
#    #packetInfo["beacon"]
#    #print packetInfo
#    pass

def rawShowPacket(packetIndex, timestamp, packet, infoTable):
    global frameAnalysis

    #print "-"*50
    #displayPacket(packetIndex, timestamp, packet)
    try:
        packetInfo = OcariPacketParse.parsePacket(packet)
    except:
        traceback.print_exc()
        displayPacket(packetIndex, timestamp, packet)
        packetInfo = {}

    if False:
        # Old hackish attempt to parse the packet
        (frameControl,), data = popStruct("H", packet)
        if frameControl & 0x3 != 0x1: return # not data
        pos = packet.find(panId) # XXX:hack
        if pos < 0: return
        data = packet[pos+len(panId):]
        if len(data) < 3: return
        if ord(data[0]) != NWK_CDE_OPERA: return
        packetLen = ord(data[1])

    #frameAnalysis.addPacket(timestamp, packetInfo)

    if "zigbee" in packetInfo:
        showZigbeePacket(packetIndex, timestamp, packet, infoTable, packetInfo)
    elif "beacon" in packetInfo:
        showBeaconPacket(timestamp, packetInfo)


def scapyShowPacket(packet, packetCount, optionTable, infoTable):
    ### Scapy-related functions:
    scapyPacket = Dot15d4(packet)
    isOperaPacket =  (ZigbeeNWKCommandPayload in scapyPacket and
                      scapyPacket.cmd_identifier == NWK_CDE_OPERA)
    #parsedPacket.show()
    #print parsedPacket.summary()
    #parsedPacket.pdfdump("%s.pdf" % packetIndex)
    #print ls(parsedPacket)
    #parsedPacket.hexdump()
    #print parsedPacket.fcf_frametype
    #if Dot15d4Data in parsedPacket:
    #   print parsedPacket.show()
    
    if optionTable.onlyOpera and not isOperaPacket:
        return

    if optionTable.verbose:
        print "-"*75
    print "%.6f"% timestamp,
    if optionTable.withPacketIndex: print "#%s" % packetCount, 
    if optionTable.withHexDump or optionTable.verbose:
        #print " ".join(["%02x" % ord(x) for x in packet])
        print "-----"
        #hexdump(scapyPacket)
        scapyPacket.show()
        if optionTable.verbose: print
        print "  --",
    try: 
        if not optionTable.onlyOpera:
            print scapyPacket.summary()
            if optionTable.verbose:
                scapyPacket.show()
    except TypeError: 
        print "***** SCAPY PARSE ERROR"
        print "   PACKET DUMP:", toHex(packet)
        print
        print traceback.format_exc()
        print
        return


    if isOperaPacket:
        size = ord(scapyPacket.data[0])
        rawPacket = scapyPacket.data[1:1+size]
        if optionTable.verbose: print
        print "  --",
        showOperaPacket(rawPacket, infoTable)

    if not optionTable.onlyOpera: print
    #print packetIndex, timestamp, d

#---------------------------------------------------------------------------

# See: http://trac.secdev.org/scapy/wiki/BuildAndDissect

#---------------------------------------------------------------------------

class PacketCapture:

    HeaderSpec = "<BIQB"
    HeaderSize = struct.calcsize(HeaderSpec)

    Coef = 32000000

    def __init__(self, port):
        self.port = port
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.bind(("", 5000))
        self.startTimestamp = None

    def getPacket(self, maxSize=65536):
        packet, fullAddress = self.s.recvfrom(maxSize)
        # see SWRU187F
        (flags, packetIndex, timestamp, size 
         ) = struct.unpack(self.HeaderSpec, packet[0:self.HeaderSize])
        packet = packet[self.HeaderSize:]
        if flags&1 != 0:
            # length includes FCS ; adjust
            size = size - 2
        payload = packet[0:size]
        fcs = packet[size:size+2]

        if self.startTimestamp == None:
            self.startTimestamp = timestamp
            self.startTime = time.time()
        else:
            pass

        actualTime = (timestamp - self.startTimestamp) /float (self.Coef)
        #print "FCS:", toHex(fcs), flags, "***"
        return (packetIndex, actualTime, payload, fcs)

    def hasPacket(self):
        rlist, wlist, xlist = select.select([self.s.fileno()], [], [], 0)
        return len(rlist) > 0
        
#---------------------------------------------------------------------------

class PacketPcap:

    HeaderSpec = "<BIQB"
    HeaderSize = struct.calcsize(HeaderSpec)

    def __init__(self, inputFileName):
        self.inputFileName = inputFileName
        #print "* Opening '%s' for reading" % inputFileName
        self.f = open(inputFileName, "rb") # -> exception if file not found
        unused = self.f.read(len(PcapHeader)) # XXX: we don't check the header
        self.packetIndex = 10000 # because it is not the same as when writing
        self.startTime = None

    def getPacket(self, maxSize=65536):
        header = self.f.read(struct.calcsize("!IIII"))
        if len(header) == 0:
            return None, None, None, None
        clockSec, clockUSec, packetSize, actualPacketSize = struct.unpack(
            "!IIII", header)
        packet = self.f.read(packetSize)
        timestamp = clockSec + (clockUSec / 1.0e6)
        fcs = chr(0)+chr(0) # XXX: garbage
        self.packetIndex += 1

        if self.startTime == None:
            self.startTime = timestamp

        return (self.packetIndex, timestamp-self.startTime, packet, fcs)

    def hasPacket(self):
        return True

#---------------------------------------------------------------------------

def setColorSeq(displayMode):
    global CSI, DCnorm, DCblue, DCgreen, DCred, DChigh
    if sys.platform in ["linux2"] and displayMode == "ansi":
        # http://en.wikipedia.org/wiki/ANSI_escape_code
        CSI = '\033'
        DCnorm = CSI + "[0m"
        DCblue = CSI + "[34m"
        DCgreen = CSI + "[32m"
        DCred = CSI + "[31m"
        DChigh = CSI + "[34m" + CSI + "[43m"
        #DCreset = CSI'[0m'
        #print CSI + "[34m"
    elif displayMode == "html":
        DCnorm =  '</font>'
        DCblue =  '<font color="blue">' 
        DCgreen = '<font color="green">' 
        DCred =   '<font color="red">' 
        #DChigh =  '<font color="yellow">'
        DChigh =  '<font color="#CC6633">'
    else:
        #"Console windows in all versions of Windows do not support 
        # ANSI escape sequences at all." -- wikipedia
        CSI = "" ; DCnorm = "" ; DCblue = "" ; DCgreen = "" ; DCred = "" 
        DChigh = ""

#---------------------------------------------------------------------------

parser = optparse.OptionParser()
parser.add_option("-v", "--verbose",
                  dest = "verbose", action="store_true", default=False)
parser.add_option("-x", "--hex-dump",
                  dest = "withHexDump", action="store_true", default=False)
parser.add_option("--no-color", dest = "displayMode", action="store_const", 
                  const = None, default="ansi")
parser.add_option("--html", dest = "displayMode", action="store_const", 
                  const = "html")
parser.add_option("-n", "--network", "--opera",
                  dest = "onlyOpera", action="store_true", default=False)
parser.add_option("--mac",
                  dest = "withMac", action="store_true", default=False)
parser.add_option("--direct", "--no-scapy",
                  dest = "noScapy", action="store_true", default=False)
parser.add_option("--packet-index",
                  dest = "withPacketIndex", action="store_true", default=False)
parser.add_option("--table-prefix",
                  dest = "tablePrefix", action="store", default=None)
parser.add_option("--table-interval", type="float",
                  dest = "tableInterval", action="store", default=None)
#parser.add_option("-p", "--port",
#                  dest = "port", type="int", default=0)
parser.add_option("--table-all",
                  dest = "withTableAll", action="store_true", default=False)

(optionTable, argList) = parser.parse_args()
option = optionTable

setColorSeq(optionTable.displayMode)

if optionTable.displayMode == "html":
    print "<html><pre>"

#--------------------------------------------------

def writeTable(prefixName, infoTable):
    data = repr(infoTable)

    tmpFileName = prefixName + ".pydata-next"
    fileName = prefixName + ".pydata"
    g = open(tmpFileName, "wb")
    g.write(data)
    g.close()
    if sys.platform == "win32":
        try: os.remove(fileName)
        except: pass
    os.rename(tmpFileName, fileName)

#--------------------------------------------------

if len(argList) == 0:
    print "*** Reading from UDP port %s" % Port

    logFileName = "ocari-packet.log"
    initialTime = time.time()
    logFileName = time.strftime("ocari-packet-%Y-%m-%d--%Hh%Mm%S.log")

    if logFileName != None:
        print "Writing pcap/wireshark log in file '%s'" % logFileName
        f = open(logFileName, "wb")
        f.write(PcapHeader)
    else: f = None

    capture = PacketCapture(Port)
    startTime = time.time()

    if not option.withMac:
        option.onlyOpera = True

    if option.tablePrefix == None:
        option.tablePrefix = stateDir+"/current-packet"

else:
    inputFileName = argList[0]
    print "*** Reading from file %s" % inputFileName
    capture = PacketPcap(inputFileName)
    f = None

#--------------------------------------------------

try: os.remove(stateDir+"/current-packet.pydata")
except: pass

class MyLog:
    def __init__(self):
        self.content = ""

    def write(self, data):
        self.content += data

    def flush(self): pass

    def retrieve(self):
        result = self.content
        self.content = ""
        return result

#oldStdout = sys.stdout
#sys.stdout = MyLog()

#--------------------------------------------------

infoTable = { "timestamp": 0 }
packetCount = 0
lastTimeStampWrite = None

frameAnalysis = OcariPacketParse.FrameAnalysis(eventNewSuperframe)

timeIndex = 0
while True:
    packetIndex, timestamp, packet, fcs = capture.getPacket()
    #displayPacket(packetIndex, timestamp, packet)
    if packetIndex == None: 
        break

    if lastTimeStampWrite == None or timestamp < lastTimeStampWrite:
        lastTimeStampWrite = timestamp

    if True:
        if lastTimeStampWrite > timestamp: 
            lastTimeStampWrite = timestamp
        if (option.tablePrefix != None 
            and (not capture.hasPacket() 
                 or (option.tableInterval != None 
                     and timestamp-lastTimeStampWrite > option.tableInterval))):
            lastTimeStampWrite = time.time()
            prefix = option.tablePrefix
            infoTable["packetIndex"] = packetIndex
            infoTable["timestamp"] = timestamp
            if option.tableInterval != None:
                prefix += ".%06d" % packetIndex
            writeTable(prefix , infoTable)


    if False and option.tablePrefix != None:
        assert option.tablePrefix != None
        assert option.tableInterval != None 

        while timestamp-lastTimeStampWrite > option.tableInterval:
            timeIndex += 1
            prefix = option.tablePrefix
            infoTable["packetIndex"] = packetIndex
            infoTable["timestamp"] = timestamp
            infoTable["timeIndex"] = timeIndex
            infoTable["htmlPacket"] = sys.stdout.retrieve()
            prefix += ".%07d" % timeIndex
            writeTable(prefix , infoTable)
            #sys.stdout.write(".") ; sys.stdout.flush()
            lastTimeStampWrite += option.tableInterval

            #oldStdout.write("-"*50 + "\n")
            oldStdout.write("%s %s\n" % (timeIndex, timestamp))
            #oldStdout.write(sys.stdout.retrieve())
            #oldStdout.write("-"*50 + "\n")
            #print timeIndex, timestamp

    packetCount += 1
    actualTime = timestamp

    if f != None:
        f.write(pcapFormatPacket(actualTime + startTime, packet + fcs))
        f.flush()

    if scapy == None or optionTable.noScapy:
        rawShowPacket(packetIndex, actualTime, packet, infoTable)
    else: 
        scapyShowPacket(packet, packetCount, optionTable, infoTable)

#---------------------------------------------------------------------------

if optionTable.displayMode == "html":
    print "</pre></html>"

#---------------------------------------------------------------------------

# python PacketDumpNew.py --table-prefix PerLog/ocari-packet-2011-09-26--10h41m02/PyState --table-interval 5   Log-2011-09-26/ocari-packet-2011-09-26--10h41m02.log  --opera

# python PacketDumpNew.py --table-prefix PerLog/ocari-packet-2011-09-26--10h41m02/PyState --table-interval 5   Log-2011-09-26/ocari-packet-2011-09-26--10h41m02.log  --opera --html

#---------------------------------------------------------------------------
