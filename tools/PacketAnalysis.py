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
elif sys.platform == "darwin":
    stateDir = "/Volumes/Ramdisk"
    print "platform MacOS-X: storing state in directory:", stateDir
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
                         int((clock-int(clock))* 1000000), # microseconds
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

def ensureNodeAddress(addr, infoTable):
    if addr not in infoTable["nodeTable"]:
        infoTable["nodeTable"][addr] = { 
            "tree": {}, "treeStatus": {}  }
    return infoTable["nodeTable"][addr]


def showOperaPacket(rawPacket, infoTable, packetInfo):
    global operaVolume
    if option.shortDisplay: packetVerbosity = 0
    else: packetVerbosity = 1
    try:
        structPacket = OperaPacketParse.parsePacket(rawPacket)
        strPacket = OperaPacketParse.reprPacket(structPacket, packetVerbosity)
    except: 
        print "PARSE ERROR************************"
        traceback.print_exc()
        #sys.exit(1)
        print "   ", toHex(rawPacket)
        return

    #print structPacket
    structPacket["header"] = packetInfo
    structPacket["timestamp"] = infoTable["timestamp"]
    packetType = structPacket.get("type", None)
    if "nodeTable" not in infoTable:
        infoTable["nodeTable"] = {}
    if packetType == "Hello":
        address = structPacket["address"]
        nodeInfo = ensureNodeAddress(address, infoTable)
        nodeInfo["lastHello"] = structPacket
    elif packetType == "STC":
        sender = structPacket["sender"]
        nodeInfo = ensureNodeAddress(sender, infoTable)
        nodeInfo["tree"][structPacket["root"]] = structPacket
    elif packetType == "TreeStatus":
        sender = structPacket["sender"]
        nodeInfo = ensureNodeAddress(sender, infoTable)
        nodeInfo["treeStatus"][structPacket["root"]] = structPacket
    elif packetType == "Color":
        sender = structPacket["sender"]
        nodeInfo = ensureNodeAddress(sender, infoTable)
        nodeInfo["lastColor"] = structPacket
    #print structPacket

    #print "%.6f" %actualTime,
    if strPacket.startswith("[Hello"): selectColor("green")
    elif strPacket.startswith("[STC"): selectColor("blue")
    elif strPacket.startswith("[Color"): selectColor("red")
    elif strPacket.startswith("[TreeS"): selectColor("high")
    print strPacket
    recordOperaPacket(infoTable["timestamp"], structPacket)
    selectColor("normal")
    operaVolume += len(rawPacket)
    #print operaVolume


MaxOperaPacketListSize = 9
operaPacketList = []
def recordOperaPacket(timestamp, structPacket):
    global operaPacketList
    packetVerbosity = 0
    try:
        strPacket = OperaPacketParse.reprPacket(structPacket, packetVerbosity)
    except: 
        return

    fs = ' size="5"'
    r = "%.6f " % timestamp
    if strPacket.startswith("[Hello"): r += '<font color="blue"'+fs+'>'
    elif strPacket.startswith("[STC"): r += '<font color="green"'+fs+'>' 
    elif strPacket.startswith("[Color"): r += '<font color="red"'+fs+'>' 
    elif strPacket.startswith("[TreeS"): r += '<font color="#CC6633"'+fs+'>'
    r += strPacket
    r += '</font>'
    operaPacketList.append(r)
    if len(operaPacketList) > MaxOperaPacketListSize:
        operaPacketList.pop(0)
    writeHtmlOperaPacket(operaPacketList)


# CF 19eme octet de la trame. (12eme)
lastWasBeacon = True
def showBeaconPacket(timestamp, packetInfo):
    global lastWasBeacon, option
    if option.beaconMode == "none":
        return
    beacon = packetInfo["beacon"]["payload"]

    #packetInfo["timestamp"] = infoTable["timestamp"]
    if packetInfo["beacon"]["superframe"]["cpan"]:
        print "-"*70
        #selectColor("high")
    else: pass #selectColor("normal")
    sys.stdout.write("%.6f" % timestamp)

    sys.stdout.write(" beacon")
    if packetInfo["src"] != None:
        panId, address = packetInfo["src"]
        try: panStr = "(%04x)" % panId
        except: panStr = "()"
        sys.stdout.write(OcariPacketParse.reprAddress(address) + panStr)
        coordStr = ",".join([OcariPacketParse.reprAddress(address)
                             for address in beacon["coord-address-list"]])
        sys.stdout.write(" #%s coord=%s cycle=%sms" %
                         (beacon["cycle-seq-num"],
                          coordStr, #beacon["nb-coord"],
                          beacon["global-cycle-duration"]*0.32
                          ))
    if packetInfo["beacon"]["superframe"]["association-permit"]:
        sys.stdout.write(" assoc")
    if packetInfo["beacon"]["superframe"]["cpan"]:
        sys.stdout.write("\n     -- ")
        if packetInfo["frame-control"]["ocari"]["coloring-mode"]:
            selectColor("red")
            sys.stdout.write("coloring ")
            selectColor("normal")
        sys.stdout.write( "color-indic=%d" 
            % packetInfo["frame-control"]["ocari"]["color-indication"])

    sys.stdout.write(" " + packetInfo["beacon"]["payload"]["unparsed"]
                     + "\n")
    if ((option.beaconMode == "cpan" 
         and packetInfo["beacon"]["superframe"]["cpan"])
        or option.beaconMode == "full"):
        pprint.pprint(packetInfo)
    lastWasBeacon = True
    #pprint.pprint(packetInfo)
    #sys.stdout.write("\n")


def eventNewSuperframe(timestamp, packetInfo):
    global frameAnalysis
    sys.stdout.write(" (prev:" + "/".join(frameAnalysis.repeaterList)+") -- ")
    showBeaconPacket(timestamp, packetInfo)


def showZigbeePacket(packetIndex, timestamp, packet, infoTable, packetInfo):
    global lastWasBeacon, option
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
        if not option.shortDisplay:
            sys.stdout.write(srcLong)
    srcShort = struct.unpack("H",zigbeePayload["src"])[0]
    dstShort = struct.unpack("H",zigbeePayload["dst"])[0]
    if not option.shortDisplay:
        sys.stdout.write("(%04x->%04x)" % (srcShort, dstShort))
        sys.stdout.write(" -- ")
    showOperaPacket(data[2:packetLen+2], infoTable, packetInfo)    
    if not option.shortDisplay: print

def showMacCommandPacket(packetIndex, timestamp, packet, infoTable, packetInfo):
    sys.stdout.write("%.6f " % timestamp)
    frameControl = packetInfo["frame-control"]
    selectColor("red")
    srcLong,srcShort,dstLong,dstShort = None, None, None, None
    if frameControl["src-address-mode"] == "long":
        srcLong = "%016x" % struct.unpack("Q",packetInfo["src"][1])[0]
        srcLong = ".."+srcLong[-6:]
        sys.stdout.write(srcLong)
    elif frameControl["src-address-mode"] == "short":
        srcShort = struct.unpack("H",packetInfo["src"][1])[0]
        sys.stdout.write("%04x" % srcShort)
    else: sys.stdout.write("*")
    if frameControl["dst-address-mode"] == "long":
        dstLong = "%016x" % struct.unpack("Q",packetInfo["dst"][1])[0]
        dstLong = ".."+dstLong[-6:]
        sys.stdout.write("->"+dstLong)
    elif frameControl["dst-address-mode"] == "short":
        dstShort = struct.unpack("H",packetInfo["dst"][1])[0]

    sys.stdout.write(" -- ")

    if "association" not in infoTable:
        infoTable["association"] = {}
    command = packetInfo["MAC-command"]["command"]
    if command == "association-request":
        if dstShort != None: dstStr = "%04x" % dstShort
        else: dstStr = ""
        sys.stdout.write("association-request >>>> %s\n" % dstStr)
        if srcLong not in infoTable["association"]:
            infoTable["association"][srcLong] = { "request": [],
                                                  "response": [] }
        assocReq = { "dstShort": dstShort, "timestamp": timestamp }
        infoTable["association"][srcLong]["request"].append(assocReq)
        #print assocReq
    elif command == "beacon-request":
        sys.stdout.write("beacon-request\n")
    elif command == "association-response":
        srcShort = struct.unpack("H",packetInfo["MAC-command"]["address"])[0]
        sys.stdout.write("association-response %s addr=%04x\n" %
                         (packetInfo["MAC-command"]["status"], srcShort))
        if dstLong not in infoTable["association"]:
            infoTable["association"][dstLong] = { "request": [],
                                                  "response": [] }
        assocResp = { "address": srcShort, 
                      "srcLong": srcLong,
                      "timestamp": timestamp }
        infoTable["association"][dstLong]["response"].append(assocResp)
        #pprint.pprint(packetInfo)
    else:
        sys.stdout.write("MAC-command=%s payload=%s" 
                         % (command, packetInfo["MAC-command"]["payload"]))

    selectColor("normal")
    #pprint.pprint(packetInfo)
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
        if "beacon" not in infoTable:
            infoTable["beacon"] = {}
        packetInfo["timestamp"] = timestamp
        if len(packetInfo["src"][1]) == 2:
            srcShort = struct.unpack("H",packetInfo["src"][1])[0]
            infoTable["beacon"][srcShort] = packetInfo
    elif "MAC-command" in packetInfo:
        showMacCommandPacket(packetIndex, timestamp, packet, 
                             infoTable, packetInfo)

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
        showOperaPacket(rawPacket, infoTable, {})

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
    if sys.platform in ["linux2", "darwin"] and displayMode == "ansi":
        # http://en.wikipedia.org/wiki/ANSI_escape_code
        CSI = '\033'
        DCnorm = CSI + "[0m"
        DCblue = CSI + "[1m" + CSI + "[34m"
        DCgreen = CSI + "[1m" + CSI + "[32m"
        DCred = CSI + "[1m" + CSI + "[31m"
        DChigh = CSI + "[1m" + CSI + "[34m" #+ CSI + "[43m"
        #DCreset = CSI'[0m'
        #print CSI + "[34m"
        
        DCgreen = CSI + "[1m" + CSI + "[34m"
        DCblue = CSI + "[1m" + CSI + "[32m"


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
parser.add_option("--beacon", dest = "beaconMode", action="store",
                  const = None, default="summary")
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
parser.add_option("--short",
                  dest = "shortDisplay", action="store_true", default=False)

(optionTable, argList) = parser.parse_args()
option = optionTable

if option.shortDisplay:
    option.beaconMode = "none"

setColorSeq(optionTable.displayMode)

if optionTable.displayMode == "html":
    print "<html><pre>"

#--------------------------------------------------

def rewriteFile(fileName, content):
    tmpFileName = fileName + "-tmp-next"
    g = open(tmpFileName, "wb")
    g.write(content)
    g.close()
    if sys.platform == "win32":
        try: os.remove(fileName)
        except: pass
    os.rename(tmpFileName, fileName)

def writeTable(prefixName, infoTable):
    data = repr(infoTable)
    rewriteFile(prefixName + ".pydata", data)

def writeHtmlOperaPacket(operaPacketList):
    global optionTable
    if optionTable.displayMode == "html": return
    refreshTime = 0.1
    #htmlLastPacket = XXX;
    htmlLastPacket = "<br>".join(operaPacketList)
    r = '''<html><head><META http-equiv="Refresh" content="%s"></head>
         <body>%s</body></html>''' % (refreshTime, htmlLastPacket)

    r2 = '''<html><head></head>
         <body>%s</body></html>''' % (htmlLastPacket,)

    fileName = stateDir+"/last-packet.html"
    rewriteFile(fileName, r)

    fileName2 = stateDir+"/last-packet-no-refresh.html"
    rewriteFile(fileName, r2)


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

if option.tableInterval != None:
    oldStdout = sys.stdout
    sys.stdout = MyLog()
    sys.stderr = sys.stdout

#--------------------------------------------------

infoTable = { "timestamp": 0 }
packetCount = 0
lastTimeStampWrite = None
lastTimeStamp = None

frameAnalysis = OcariPacketParse.FrameAnalysis(eventNewSuperframe)

writeHtmlOperaPacket([])

timeIndex = 0
offset = 0

while True:
    packetIndex, timestamp, packet, fcs = capture.getPacket()
    #displayPacket(packetIndex, timestamp, packet)
    if packetIndex == None: 
        break

    timestamp += offset
    if lastTimeStamp != None and timestamp < lastTimeStamp:
        offset += (lastTimeStamp - timestamp)
        timestamp = lastTimeStamp
    lastTimeStamp = timestamp

    if lastTimeStampWrite == None or timestamp < lastTimeStampWrite:
        lastTimeStampWrite = timestamp

    if option.tableInterval == None:
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

    if (option.tableInterval != None and option.tablePrefix != None):
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

print "(end of trace)"

if optionTable.displayMode == "html":
    print "</pre></html>"


#---------------------------------------------------------------------------

# python PacketDumpNew.py --table-prefix PerLog/ocari-packet-2011-09-26--10h41m02/PyState --table-interval 5   Log-2011-09-26/ocari-packet-2011-09-26--10h41m02.log  --opera

# python PacketDumpNew.py --table-prefix PerLog/ocari-packet-2011-09-26--10h41m02/PyState --table-interval 5   Log-2011-09-26/ocari-packet-2011-09-26--10h41m02.log  --opera --html

#---------------------------------------------------------------------------
