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
This package can be used to send some command to ZExx Telit Modules
"""

import sys, time
import struct, optparse
import serial # Windows: http://pypi.python.org/pypi/pyserial  (pyserial-2.5)
              # Linux: "sudo apt-get install python-serial"

try: 
    sys.path.append("../../pkg-python/HipSens/Core")
    import LibCommand
except: print "Failed to import LibCommand, opera commands are not available"

#print LibCommand.OperaSetCode

#Verbose = True
Verbose = False

DefaultTimeOut = 0.1
DefaultFlushDuration = 0.3
DefaultRetryDuration = 3.0

OperaCode = 0xFE

#---------------------------------------------------------------------------

class CmdException(Exception):
    def __init__(self, msg, info):
        self.msg = msg
        self.info = info

    def __str__(self):
        return "["+self.msg+": "+repr(self.info)+"]"

#--------------------------------------------------

def openSerialPort(portNumber, timeOut):
    #port = serial.Serial("/dev/ttyUSB2")
    #port.port = 0 # COM1
    if sys.platform == "linux2":
        port = serial.Serial("/dev/ttyUSB%s" % portNumber)
        port.rtscts = True # XXX: check
    elif sys.platform == "win32":
        port = serial.Serial()
        port.port = portNumber
        port.rtscts = False # XXX: check
    elif sys.platform == "darwin":
        if portNumber != 0:
            raise serial.SerialException("only one USB port is supported on MacOS-X")
        port = serial.Serial("/dev/tty.usbserial")
        port.rtscts = True
    else:
        print "platform:", sys.platform
        raise RuntimeError("Unknown Platform", sys.platform)
    port.baudrate = 115200
    port.bytesize = serial.EIGHTBITS
    port.stopbits = serial.STOPBITS_ONE
    port.parity = serial.PARITY_NONE

    port.timeout = timeOut # sec timeout on read
    port.open()
    return port

#--------------------------------------------------

# II.3.1 - Primitives
CmdSetRequest = 0x12
CmdSetConfirm = 0x13
CmdGetRequest = 0x14
CmdGetConfirm = 0x15

# II.3.2 - Attributes
AttributeNameList = [
    ("IEEE Address", 0x6F),
    ("RxOnWhenIdle", 0x52),
    ("Sleeping Time", 0x56),
    ("Radio Channel", 0x01),
    ("Current Channel", 0x00),
    ("Version Stack", 0x04),
    ("Version Bootloader", 0x05),
    ("Version Application", 0x0A),
    ("ExtendedPanID", 0xC4),
    ("TrustCenter", 0xCA),
    ("USB device", 0x99),
    ("Type of device", 0x06),
    ("Is associated", 0x07),
    ("Nwk address", 0x96),
    ("Fragmentation Inter Frame Delay", 0xC9),
    ("Fragmentation Window Size", 0xCD)
    # <security attribute not put here yet>
]
AttributeNameTable = dict(AttributeNameList)

DeviceTypeName = {
    0x10: "CPAN",
    0x11: "Router",
    0x12: "End-Device"
}

def toHex(data, sep = " "):
    return sep.join(["%02x" % ord(x) for x in data])


class ModuleException(Exception):
    def __init__(self, *info):
        self.info = info
    def __str__(self):
        return repr(self.info)

class ZExxModule:
    def __init__(self, name, port = 0, verbose=False, 
                 timeOut = DefaultTimeOut, option = None):
        self.name = name
        self.portIndex = port
        self.verbose = verbose
        self.timeOut = timeOut
        self.option = option
        self.port = None

    def open(self):
        self.port = openSerialPort(self.portIndex, self.timeOut)
    
    def write(self, data):
        if self.verbose:
            sys.stdout.write(self.name + " -> "
                             + " ".join(["%02x" % ord(x) for x in data])
                             +"\n")
        self.port.write(data)
        
    def directReadOne(self):
        return self.port.read(1)

    def read(self, length):
        if not self.verbose:
            result = self.port.read(length)
        else:
            sys.stdout.write(self.name + " <- ")
            sys.stdout.flush()
            result = ""
            for i in range(length):
                c = self.port.read(1)
                result += c
                if c != "":
                    sys.stdout.write(" %02x" % ord(c))
                    sys.stdout.flush()
            sys.stdout.write("\n")
        if len(result) < length:
            raise ModuleException("read too short", 
                                  "%s < %s" % (len(result), length))
        return result

    def readFrame(self, expectedCommand = None):
        frameLength = self.read(1)
        if len(frameLength) != 1:
            raise CmdException(
                "[frame] invalid frameLength field", frameLength)
        (frameLength,) = struct.unpack("B", frameLength)
        data = self.read(frameLength)
        if len(data) != frameLength:
            raise CmdException("[frame] incorrect amount of data", 
                               (frameLength,len(data), data))
        if expectedCommand != None:
            if len(data) < 1:
                raise CmdException("[frame] expected command but empty data","")
            cmd, = struct.unpack("B", data[0:1])
            if cmd != expectedCommand:
                raise CmdException( "[frame] expected/actual command mismatch",
                                    (cmd, expectedCommand))
        return data

    def oldOperaCommand(self):
        CmdOpera = 0xfe
        data = "\x44\x77"
        cmd = struct.pack("BB", 1+len(data), CmdOpera) + data
        self.write(cmd)
        self.flush()

    def directOperaCommand(self, argList):
        cmd = LibCommand.makeCommand(*argList)
        self.write(cmd)
        self.flush()
        

    def operaCommand(self, argList):
        cmd = LibCommand.makeCommand(*argList)
        self.write(cmd)

        frameLength, = struct.unpack("B",self.read(1))
        code = struct.unpack("B",(self.read(1)))
        result = self.read(max(frameLength-1,0))
        return code, result


    def permitJoining(self, address, permitDuration, tcSignifiance = 0x0):
        cmd = struct.pack("BBHBB", 0x5, 0xEA, address, permitDuration, 
                          tcSignifiance)
        self.write(cmd)

        frameLength, = struct.unpack("B",self.read(1))
        code = struct.unpack("B",(self.read(1)))
        result = self.read(max(frameLength-1,0))
        return code, result

    def sendFrameDirect(self, dst, data, withAck = True):
        # See II.4.20 "Application Frame Direct"
        txOption = 0
        if withAck: txOption = txOption | (1<<2) # see doc
        cmd = struct.pack("<BBHBBHB",
                          10+len(data), # FrameLength
                          0xF3,         # Command
                          dst,          # DstAddress
                          0xcc, 0xbb,   # DstEndPoint, SrcEndPoint
                          0xface,       # ClusterId
                          len(data))    # afduLength
        cmd += data
        cmd += struct.pack("BB",
                           txOption,     # txOption
                           0x11)
        self.write(cmd)
        
        frameLength, = struct.unpack("B",self.read(1))
        code = struct.unpack("B",(self.read(1)))
        result = self.read(max(frameLength-1,0))
        return code, result
        

    def exitConfigMode(self):
        # cf [ZeDoc] II.2 p6
        cmd = struct.pack("BB", 0x01, 0x00) 
        expectedAnswer = struct.pack("BBB", 0x02, 0x00, 0x00)
        self.write(cmd)
        answer = self.read(len(expectedAnswer))
        if answer != expectedAnswer:
            raise CmdException("unexpected answer to 'exit config mode'",
                               answer)

    def doGetRequest(self, requestedAttribute):
        # [ZeDoc] II.3.1
        # Send 'Get Request'
        cmd = struct.pack("BBB", 2, CmdGetRequest, requestedAttribute)
        self.write(cmd)
        # Receive/Parse 'Get Confirm'
        data = self.readFrame(CmdGetConfirm)
        (cmd, status) = struct.unpack("BB", data[0:2])
        if status != 0x00:
            return None # Not OK - XXX: maybe raise exception
        (attribute, attributeLength) = struct.unpack("BB", data[2:4])
        value = data[4:]
    
        if attribute != requestedAttribute:
            raise CmdException( "returned attribute is not requested attribute",
                                (requestedAttribute, attribute))
        if attributeLength != len(value):
            print CmdException("inconsistent returned attribute length",
                               (attributeLength, len(value)))
        return value

    def doSetRequest(self, requestedAttribute, rawAttribute):
        # [ZeDoc] II.3.1
        # Send 'Set Request'
        #cmd = struct.pack("BBB", 2+len(rawAttribute),
        #                  CmdSetRequest, requestedAttribute)
        cmd = struct.pack("BBB", 2+len(rawAttribute)+1, 
                          CmdSetRequest, requestedAttribute)
        cmd += struct.pack("B", len(rawAttribute)) + rawAttribute
        self.write(cmd)
        # Receive/Parse 'Get Confirm'
        data = self.readFrame(CmdSetConfirm)
        (cmd, status) = struct.unpack("BB", data[0:2])
        if status != 0x00:
            return None # Not OK - XXX: maybe raise exception
        (attribute, attributeLength) = struct.unpack("BB", data[2:4])
        value = data[4:]
    
        print value
        return value


    def dumpConfig(self):
        self.enterConfigMode()
        for name,attributeCode in AttributeNameList: #Table.items():
            #time.sleep(0.2)
            #value = repr(ze.doGetRequest(attributeCode))
            value = repr(self.safeDoGetRequest(attributeCode))
            print name, value
    
    def safeDoGetRequest(self, attributeCode):
        return self.safeRetry(ze.doGetRequest, [attributeCode])

    def doStart(self):
        # DR11 p5 - II.4.1
        # Send Start request
        CmdStartRequest = 0x16
        CmdStartConfirm = 0x17
        cmd = struct.pack("BB", 1, CmdStartRequest)
        self.write(cmd)
        # Get Start confirm
        data = self.readFrame(CmdStartConfirm)
        if len(data) != 2:
            raise CmdException(
                "incorrect 'Start confirm' length", (2, len(data), data))
        (cmd, status) = struct.unpack("BB", data[0:2])
        return status

    def doReset(self, hardReset = False):
        # II.5.2
        # Send Reset Request
        CmdReset = 0x10
        CmdResetConfirm = 0x11
        if hardReset: value = 0x00
        else: value = 0x01 # 'soft reset (disconnect from network)'
        cmd = struct.pack("BBB", 2, CmdReset, value)
        self.write(cmd)

        # Get Reset Confirm
        data = self.readFrame(CmdResetConfirm)
        if len(data) != 2:
            raise CmdException(
                "incorrect 'Reset confirm' length", (2, len(data), data))
        (cmd, status) = struct.unpack("BB", data[0:2])
        return status

    def safeRetry(self, command, args, timeout = None):
        startTime = time.time()
        # Send/resend a command until there is a answer
        while timeout == None or time.time()-startTime < timeout:
            try: return command(*args)
            except ModuleException:
                time.sleep(0.1)
                sys.stdout.write("[@]")
        raise RuntimeError("Timeout", command, args)

    def flush(self):
        data = ""
        while True:
            try: data += ze.read(1)
            except ModuleException: break
        return data

    #--------------------------------------------------

    def longFlush(self, duration):
        data = ""
        startTime = time.time()
        while time.time()-startTime < duration:
            while True:
                try: data += ze.read(1)
                except ModuleException: break
        return data

    def safeRetryAndFlush(self, command, args):
        startTime = time.time()
        # Send/resend a command until there is a answer
        while time.time()-startTime < self.option.retryDuration:
            try: return command(*args)
            except ModuleException: pass
            #time.sleep(option.flushDelay)
            self.longFlush(self.option.flushDuration)
            sys.stdout.write("@")
            
        raise RuntimeError("Timeout", command, args)

    def enterConfigMode(self):
        # cf [ZeDoc] II.2 p6
        self.write("+++")
        answer = self.read(1)
        if answer != chr(0x0d):
            raise CmdException("unexpected answer to 'enter config mode'",
                               answer)
        return True
    
    def getDeviceInfo(self):
        address = self.safeRetryAndFlush(
            self.doGetRequest, [AttributeNameTable["IEEE Address"]])
        deviceType = self.safeRetryAndFlush(
            self.doGetRequest, [AttributeNameTable["Type of device"]])
        return (address, ord(deviceType))

    def setAddressFromPort(self):
        address = self.safeRetryAndFlush(
            self.doGetRequest, [AttributeNameTable["IEEE Address"]])
        address = address[:-1] + chr(self.portIndex+1)
        result = self.safeRetryAndFlush(
            self.doSetRequest, [AttributeNameTable["IEEE Address"],
                                address])
        return result

    def getAssociationInfo(self):
        isAssociated = self.safeRetryAndFlush(
            self.doGetRequest, [AttributeNameTable["Is associated"]])
        shortAddress = self.safeRetryAndFlush(
            self.doGetRequest, [AttributeNameTable["Nwk address"]])
        #print repr(isAssociated), repr(shortAddress)
        return (ord(isAssociated) != 0, struct.unpack("!H",shortAddress))

    def getTreeSeqNum(self):
        isAssociated = self.safeRetryAndFlush(
            self.doGetRequest, [AttributeNameTable["Is associated"]])
    

#---------------------------------------------------------------------------

def getAvailablePortList():
    portList = []
    for port in range(10):
        try: serialPort = openSerialPort(port, DefaultTimeOut)
        except serial.SerialException: continue
        print "Serial port #%s found (%s)"  % (port, serialPort.port)
        portList.append(port)
    return portList

def doScanComPort(optionTable, argList):
    for port in range(10):
        try: serialPort = openSerialPort(port, DefaultTimeOut)
        except serial.SerialException: continue
        print "Serial port #%s found (%s)"  % (port, serialPort.port)

#---------------------------------------------------------------------------

def runAsProgram(progArgList, verbose = Verbose):
    global ze

    parser = optparse.OptionParser()
    parser.add_option("-v", "--verbose",
                      dest = "verbose", action="store_true", default=verbose)
    parser.add_option("-p", "--port",
                      dest = "port", type="int", default=0)
    parser.add_option("-t", "--time-out",
                      dest = "timeOut", type="float", default=DefaultTimeOut)
    parser.add_option("-f", "--flush", dest = "flushDuration", type="float", 
                      default=DefaultFlushDuration)
    parser.add_option("-r", "--retry", dest = "retryDuration", type="float",
                      default=DefaultRetryDuration)

    if verbose: print "Command arguments", progArgList

    (optionTable, argList) = parser.parse_args(progArgList)

    if len(argList) > 0: 
        name = argList[0]
        argList = argList[1:]
    else: name = "scan"

    ze = ZExxModule("[serial/%s]" % optionTable.port, optionTable.port, 
                    optionTable.verbose,
                    optionTable.timeOut, optionTable)

    if name == "scan":
        doScanComPort(optionTable, argList[1:])

    elif name == "dump":
        print "<getting information of lots of variables>"
        ze.open()
        ze.enterConfigMode()
        ze.dumpConfig()

    elif name == "start":
        print "<start network (not working???)>"
        ze.open()
        ze.enterConfigMode()
        #ze.safeRetry(ze.doStart, [])
        print ze.doStart()

    elif name == "reset" or name == "soft-reset":
        print "<perform a soft-reset of the node ; use 'HayesMode' afterwise>"
        ze.open()
        ze.enterConfigMode()
        ze.safeRetry(ze.doReset, [False])

    elif name == "hard-reset":
        print "<perform a hard-reset of the node ; use 'HayesMode' afterwise>"
        ze.open()
        ze.enterConfigMode()
        ze.safeRetry(ze.doReset, [True])

    elif name == "flush":
        ze.open()
        data = ze.flush()
        #print "FLUSHED:", toHex(data)

    elif name == "cat":
        ze.verbose = True
        ze.open()
        while True:
            data = ze.flush()
            #if len(data) > 0 :
            #    print "   :", toHex(data)

    elif name == "read":
        ze.open()
        print ze.directReadOne()

    elif name == "simple-enter-config-mode":
        ze.open()
        ze.enterConfigMode()

    #--------------------------------------------------

    elif name == "enter-config-mode":
        print "<sending '+++' to enter Hayes mode>"
        ze.open()
        result = ze.safeRetryAndFlush(ze.enterConfigMode, [])
        print " ----- ", result

    elif name == "device-info":
        print "<getting device information: IEEE MAC address and device type>"
        ze.open()
        address, deviceType = ze.getDeviceInfo()
        print "Address:", toHex(address, ":")
        print "Device:", DeviceTypeName.get(deviceType, ("Unknown", deviceType))

    elif name == "association-info":
        print "<checking if node is associated and get short address>"
        ze.open()
        isAssociated, shortAddress = ze.getAssociationInfo()
        print "isAssociated:", isAssociated
        print "shortAddress:", "0x%x" % shortAddress

    elif name == "set-mac-address":
        ze.open()
        result = ze.setAddressFromPort()
        print " ----- ", result


    elif name == "opera":
        ze.open()
        code, result = ze.operaCommand(argList)
        print " ----- 0x%x" % code,
        print toHex(result)


    elif name == "simple-opera":
        ze.open()
        ze.directOperaCommand(argList)

    elif name == "permit-joining":
        ze.open()
        address = eval(argList[0])
        if len(argList) > 1:
            permitDuration = eval(argList[1])
        else: permitDuration = 0
        ze.permitJoining(address, permitDuration)
        exitNow
        #address = argList[1]
        #def permitJoining(self, address, permitDuration, tcSignifiance = 0x0):

    elif name == "send":
        ze.open()
        address = eval(argList[0])
        data = argList[1]
        if len(argList) > 2:
            withAck = eval(argList[2])
        ze.sendFrameDirect(address, data, withAck = True)

    elif name == "long-flush":
        print "<emptying and dumping serial port>"
        ze.open()
        data = ze.longFlush(optionTable.flushDuration)
        print " ----- ", toHex(data)

    else: raise ValueError("Unknown command", name)

    if optionTable.flushDuration != None and ze.port != None:
        ze.timeOut = optionTable.flushDuration
        ze.flush()

#---------------------------------------------------------------------------

if __name__ == "__main__":
    runAsProgram(sys.argv[1:])

#---------------------------------------------------------------------------

if 0:
    ze = ZExxModule("CPAN-0")
    print "+ open"
    ze.open()
    print "+ enterConfigMode"
    ze.enterConfigMode()
    ze.dumpConfig()

#print "+ doReset(hardReset = True)"
#print ze.doReset(True)

#print ze.doReset(False)

#print "+ doStart()"
#print ze.doStart()

#print ze.doStart()
#print repr(ze.doGetRequest(0x6f))

#---------------------------------------------------------------------------
