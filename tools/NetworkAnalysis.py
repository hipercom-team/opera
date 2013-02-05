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

import sys, pprint, subprocess, traceback, os, stat, time, optparse
import tempfile, webbrowser

#NeighExpireTime = 5*3.5
#TreeExpireTime = 5*3.5
#ColorExpireTime = 5*3.5

NeighExpireTime = 2*5*3.5
TreeExpireTime  = 2*5*3.5
ColorExpireTime = 2*5*3.5

#---------------------------------------------------------------------------

GraphColorTable = {
    "11": "GreenYellow",
    "12": "Tan",
    "13": "PeachPuff",
    "14": "CornflowerBlue",
    "15": "LightBlue",
    "16": "PaleGreen",
    "17": "Chartreuse"
}

GraphColorList = ["yellow", "GreenYellow", "Tan", "PeachPuff", 
                  "CornflowerBlue",
                  "LightBlue", "LightSeaGreen",
                  "PaleGreen", "Chartreuse"]

#---------------------------------------------------------------------------

if sys.platform == "win32": 
    stateDir = tempfile.gettempdir()
    print "platform win32: storing state in directory:", stateDir
else: 
    stateDir = "/dev/shm"
    print "platform linux: storing state in directory:", stateDir


if sys.platform == "win32":
    dotPath = None
    pathSuffix = ':\\Program Files\\Graphviz 2.28'
    for prefix in ['C', 'D', 'E', 'F']:
        if os.path.exists(prefix+pathSuffix+'\\bin\\dot.exe'):
            dotPath = prefix + pathSuffix +'\\bin\\dot.exe'
    if dotPath == None:
        print "FATAL: Cannot find 'dot.exe' in C,D,E,F" + pathSuffix
        print "- please install graphviz"
        time.sleep(1000)
        sys.exit(1)
else: dotPath = "dot"

#---------------------------------------------------------------------------

def getDotOutput(graphAsDot, progName = dotPath, format = "png"):
    try:
        ps = subprocess.Popen([progName, "-T"+format], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE)
        ps.stdin.write(graphAsDot)
        ps.stdin.close()
        out = ps.stdout.read()
        print len(out)
    except:
        print "FATAL: Cannot run 'dot' as", progName
        traceback.print_exc()
        out = ""

    print graphAsDot
    return out

def runDot(graphAsDot, dotFileName, graphFileName, 
           progName = dotPath, format = "png"):
    #dotFileName  = stateDir + "/current-packet.dot"
    #graphFileName = stateDir + "/current-state.png"

    f = open(dotFileName, "w")
    f.write(graphAsDot)
    f.close()

    try:
        code = subprocess.call([progName, "-T"+format, "-o", graphFileName, 
                                dotFileName])
        assert code == 0
    except:
        print "FATAL: Cannot run 'dot' as", progName
        traceback.print_exc()
        out = ""
    #print graphAsDot
    #return out

#---------------------------------------------------------------------------

def ra(x):
    return "0x%x" % x

class SnapshotFormatter:
    def __init__(self, snapshotFileName):
        self.snapshotFileName = snapshotFileName
        self.readInfo()

    def readInfo(self):
        f = open(self.snapshotFileName)
        self.infoTable = eval(f.read())
        f.close()

    def makeGraph(self):
        r = "digraph G {\n"

        clock = self.infoTable["timestamp"]
        if "nodeTable" not in self.infoTable:
            r += "}\n"
            return r

        treeTable = {}

        treeSeqNum = {}
        for nodeId in self.infoTable["nodeTable"]:
            nodeInfo = self.infoTable["nodeTable"][nodeId]
            if "tree" not in nodeInfo:
                continue

            for treeRoot in nodeInfo["tree"]:
                treeInfo = nodeInfo["tree"][treeRoot]
                if treeRoot not in treeSeqNum:
                    treeSeqNum[treeRoot] = treeInfo["treeSeqNum"]
                treeSeqNum[treeRoot] = max(treeSeqNum[treeRoot], 
                                           treeInfo["treeSeqNum"])

        for nodeId in self.infoTable["nodeTable"]:
            nodeInfo = self.infoTable["nodeTable"][nodeId]
            if "tree" not in nodeInfo:
                continue

            for treeRoot in nodeInfo["tree"]:
                treeInfo = nodeInfo["tree"][treeRoot]
                if clock - treeInfo["timestamp"] > TreeExpireTime:
                    continue

                if treeSeqNum[treeRoot] > treeInfo["treeSeqNum"]: continue

                treeId = ra(treeInfo["root"])+"_%s"%treeInfo["treeSeqNum"]
                #print treeId
                optionList = ['label="%s /%s"' % (ra(nodeId), treeInfo["cost"])]

                if "flag" in treeInfo:
                    if "stable" in treeInfo["flag"]:
                        optionList.append("color=green")
                    elif "colored" in treeInfo["flag"]:
                        optionList.append("color=blue")

                if (nodeId == treeRoot):
                    optionList.append("shape=doublecircle")
                r += '  n%s%s [%s];\n' %  (
                    nodeId, treeId, ",".join(optionList))
                if nodeId != treeInfo["parent"]:
                    r += '  n%s%s -> n%s%s;\n' % (nodeId, treeId,
                                                  treeInfo["parent"], treeId)
                #r += ('  %s [shape=record,%s label="%s"]; \n' 
                #      % (nodeId, opt, label))

                #label = '{' + "|".join(labelList) + '}'
                

        for nodeId in self.infoTable["nodeTable"]:
            nodeInfo = self.infoTable["nodeTable"][nodeId]
            if "lastHello" not in nodeInfo: continue
            lastHello = nodeInfo["lastHello"]
            labelList = [ ra(nodeId) ]
            if clock - lastHello["timestamp"] > NeighExpireTime:
                opt = ["color=gray"]
                expired = True
            else: 
                expired = False
                opt = ["style=filled"]

                if ("lastColor" in nodeInfo 
                    and clock - nodeInfo["lastColor"]["timestamp"] <= ColorExpireTime):
                    lastColor = nodeInfo["lastColor"]
                    if ("color" in lastColor 
                        and lastColor["color"] != None):
                        colorName = GraphColorList[lastColor["color"]
                                                   % len(GraphColorList)]
                        opt = ["style=filled,fillcolor=%s" % colorName]
                        colorStr = "c=%s" % lastColor["color"]
                    else: colorStr = ""

                    if ("maxColor" in lastColor 
                        and lastColor["maxColor"] != None):
                        colorStr += "/%s" % lastColor["maxColor"]
                    if "priority" in lastColor:
                        colorStr += " prio=%s" % lastColor["priority"]
                    labelList.append(colorStr)

                    maxPrioInfo = None
                    if "maxPrio1" in lastColor:
                        if len(lastColor["maxPrio1"]) > 0:
                            prio, nodeAddr = lastColor["maxPrio1"][0]
                            maxPrioInfo = "%s:%s" % (prio, ra(nodeAddr))
                    if "maxPrio2" in lastColor:
                        if len(lastColor["maxPrio2"]) > 0:
                            prio, nodeAddr = lastColor["maxPrio2"][0]
                            if maxPrioInfo == None: maxPrioInfo = ""
                            maxPrioInfo += " // "
                            maxPrioInfo += "%s:%s" % (prio, ra(nodeAddr))
                    if maxPrioInfo != None: labelList.append(maxPrioInfo)

            opt = ",".join(opt)
            label = '{' + "|".join(labelList) + '}'
            r += ('  %s [shape=record,%s label="%s"]; \n' 
                  % (nodeId, opt, label))

            if expired: continue
            for linkType, addressList in lastHello["linkInfo"]:
                for otherNodeId in addressList:
                    if linkType == "asym":
                        r += ("  %s -> %s [style=dotted];\n" 
                              % (nodeId, otherNodeId))
                    elif linkType == "sym":
                        r += "  %s -> %s;\n" % (nodeId, otherNodeId)
                    elif linkType == ord('S') or linkType == ord('M'):
                        pass
                    else: raise ValueError("Unknown linkType", linkType)

        r += "}\n"

        #print r

        return r

        #print nodeId
        #
        #label = ("{"
        #         + "|".join([netToStr(x) for x in allAddressOf[name]]
        #                    +["(%s)" % netToStr(x) 
        #                      for x in groupOf.get(name,[])])
        #         + "}")     

    def makeHTMLPage(self, refreshTime=None):
        r = '''<html> <head>'''
        if refreshTime != None:
            r += '<META http-equiv="Refresh" content="%s">' % refreshTime
        r += '</head>'
        r += '<body>\n'

        #r += linkText
        r += '<h1> %s </h1>' % time.time()
        r += "<img src='current-state.png'>"
        r += '</body></html>'
        return r


def writeFile(fileName, content):
    f = open(fileName, "w")
    f.write(content)
    f.close()

def periodicDisplay():
    dataFileName  = stateDir + "/current-packet.pydata"
    graphFileName = stateDir + "/current-state.png"
    htmlFileName  = stateDir + "/current-state.html"

    firstRun = True
    while True:
        try:
            lastMtime = os.stat(dataFileName)[stat.ST_MTIME]
            f = open(dataFileName)
            data = eval(f.read())
            f.close()
        except: 
            try: os.remove(graphFileName)
            except: pass
            traceback.print_exc() 
            time.sleep(2)
            continue

        formatter = SnapshotFormatter(dataFileName)
        if sys.platform != "win32":
            pngOutput = getDotOutput(formatter.makeGraph())
            writeFile(graphFileName+"-tmp", pngOutput)
        else:
            dotInfo = formatter.makeGraph()
            runDot(dotInfo, stateDir + "/current-state.dot", 
                   graphFileName+"-tmp")

        if sys.platform == "win32":
            try: os.remove(graphFileName)
            except: pass
            try: os.remove(htmlFileName)
            except: pass
        os.rename(graphFileName+"-tmp", graphFileName)

        writeFile(htmlFileName+"-tmp", formatter.makeHTMLPage(refreshTime=1))
        os.rename(htmlFileName+"-tmp", htmlFileName)

        try: newMtime = os.stat(dataFileName)[stat.ST_MTIME]
        except: newMtime = None
        
        if newMtime != lastMtime:
            lastMtime = newMtime
            sys.stdout.write("*")
            sys.stdout.flush()
        else: 
            sys.stdout.write(".")
            time.sleep(2)

        if firstRun == True:
            firstRun = False
            print htmlFileName
            webbrowser.open("file:" + htmlFileName)

def formatDirectory(dirName, option):
    resultDirName = dirName+"-result"
    fileNameList = [fileName for fileName in os.listdir(dirName)
                    if fileName.endswith("pydata")]
    fileNameList.sort()
    try: os.mkdir(resultDirName)
    except: pass
    try: os.mkdir(resultDirName + "/image")
    except: pass

    s1 = 5 ; s2 = 20
    L = 5
    for i, fileName in enumerate(fileNameList):
        formatter = SnapshotFormatter(dirName +"/"+fileName)
        pngFileName = fileName.replace(".pydata", ".png")
        if not option.noImage:
            r = formatter.makeGraph()
            pngOutput = getDotOutput(r)
            writeFile(resultDirName + "/image/" + pngFileName, pngOutput)

        nextLink = None
        previousLink = None
        linkTable = []
        for di in [None, -s2, -s1, -1, 0, +1, +s1, +s2]:
            if di == None: j = 0
            else: j = i + di
            if 0 <= j and j < len(fileNameList):
                if di != None and di >= 0: diStr = "+%s" % (di*L)
                elif di == None: diStr = "[start]"
                else: diStr = "%s" % (di*L)
                otherFileName = fileNameList[j]
                otherHtmlFileName = otherFileName.replace(".pydata", ".html")
                link = '<a href="%s">%s</a>' % (otherHtmlFileName, diStr)
                linkTable.append(link)
                if di == 1: nextLink = otherHtmlFileName
                if di == -1: previousLink = otherHtmlFileName

        #print linkTable
        infoTable = formatter.infoTable
        linkText = " ".join(linkTable)
        r = '<html>\n'
        r += '<body>\n'
        r += '<a href="%s">&gt;&gt;</a>' % nextLink
        r += '&nbsp;&nbsp;<a href="%s">&lt;&lt;</a>' % previousLink
        r += '&nbsp;&nbsp;<a href="../comment.html">IDX</a>'
        #r += '&nbsp;&nbsp;<a href="../../">REF</a>'
        #r += '</br>'
        r += "&nbsp;&nbsp;"
        r += linkText
        r += '<h1> [%s] </h1>' % (infoTable["timestamp"]) # dirName
        r += '<img src="image/%s">' % pngFileName
        r += '<h2> Previous packets </h2><pre>'
        r += infoTable["htmlPacket"]
        r += '</pre></body></html>'

        htmlFileName = fileName.replace(".pydata", ".html")
        writeFile(resultDirName + "/" + htmlFileName, r)


parser = optparse.OptionParser()
parser.add_option("--no-image",
                  dest = "noImage", action="store_true", default=False)
(optionTable, argList) = parser.parse_args()
option = optionTable

if len(sys.argv) > 1 and sys.argv[1] == "real-time" or len(sys.argv) == 1:
    try:
        periodicDisplay()
    except: 
        traceback.print_exc()
        print "(please press Ctrl-C to interrupt)"
        time.sleep(1000) # for console Windows

elif len(sys.argv) > 1:
    if not os.path.isdir(sys.argv[1]):
        raise ValueError("Not a directory")
    else: formatDirectory(sys.argv[1], optionTable)

if __name__ == "__main__" and False:

    fileName = "Analysis/state.010288.pydata"
    fileName = "Analysis/state.013534.pydata"
    fileName = "Analysis/state.019990.pydata"
    fileName = "Analysis/state.012734.pydata"
    formatter = SnapshotFormatter(fileName)

    pprint.pprint(formatter.infoTable)

    r = formatter.makeGraph()
    pngOutput = getDotOutput(r)
    f = open("a.png", "w")
    f.write(pngOutput)
    f.close()

# python FormatSnapshotNew.py PerLog/ocari-packet-2011-09-26--10h41m02/PyState --no-image
# python FormatSnapshotNew.py PerLog/ocari-packet-2011-09-26--14h26m52/PyState --no-image
