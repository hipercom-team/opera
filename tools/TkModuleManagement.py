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

import sys, traceback, time
from Tkinter import *
import ModuleManagement

#---------------------------------------------------------------------------

ColumnSize = 80
LineSize = 50

CommandList = [
    ("enter-command-mode", "HayesMode", ["enter-config-mode"]),
    ("device-info", "DeviceInfo", ["device-info"]),
    #("device-info", "DeviceInfo", ["device-info"])
    ("association-info", "Assoc.", ["association-info"]),
    ("flush", "Flush", ["long-flush"]),
    ("dump", "Dump", ["dump"]),
    ("start", "Start", ["start"]),
    ("soft-reset", "Soft Reset", ["soft-reset"]),
    ("hard-reset", "Hard Reset", ["hard-reset"]),
    (None,None,None),
    ("eond-start", "EOND Start", ["opera", "set", "eond-start-delay", "0"]),
    ("start-stc", "[!]STC", ["opera", "start-eostc", "0"]),
    ("start-stc-colored", "[!!!]ColSTC", ["opera", "start-eostc", "1"]),
    ("next-stc-colored", "[!!!]IncSTC", ["opera", "next-eostc"]),
    ("tree-seq-num", "RootTreeSeq", ["opera", "get", "tree-seqnum"]),
    (None,None,None),
    ("no-filter", "No Filter", ["opera", "address-filter", "none"]),
    ("filter-0", "Filt Only 0", ["opera", "address-filter", "accept", "0"]),
    ("filter-1", "Filt Only 1", ["opera", "address-filter", "accept", "1"]),
    ("filter-2", "Filt Only 2", ["opera", "address-filter", "accept", "2"]),

    ("filter-no-0", "Filt No 0", ["opera", "address-filter", "reject", "0"]),
    ("filter-no-1", "Filt No 1", ["opera", "address-filter", "reject", "1"]),
    ("filter-no-2", "Filt No 2", ["opera", "address-filter", "reject", "2"]),
#    (None,None,None),
#    ("no-joining-1", "NoJoin x1", ["permit-joining", "0x01"]),
#    ("no-joining-2", "NoJoin x2", ["permit-joining", "0x02"]),
#    ("no-joining-f", "NoJoin xf", ["permit-joining", "0x0f"])
    #("", "GetTreeSeq", []),
] # XXX: redundancy
   
#---------------------------------------------------------------------------

class ModuleModel:
    def __init__(self):
        pass

class TextFrameAsFile:
    def __init__(self, view):
        self.view = view
        self.isStart = True

    def write(self, data):
        self.view.text.insert("end", data)
        self.isStart = data.endswith("\n")

    def ensureNewLine(self):
        if not self.isStart: self.write("\n")

    def flush(self): pass
    

class TkModuleManagement: # Presenter-View

    def __init__(self, master):
        self.master = master
        self.initModel()
        self.initView()
        #self.initPresenter()
        #print "**** WARNING - this interface is experimental ****"
        #print "     [press several times on buttons and hope for the best]"
        #print "TODO: This application will freeze under Windows"
        #print "       currently only available for Linux"

    #--------------------------------------------------
        
    def initView(self):
        self.frame = Frame(self.master)
        self.frame.pack()

        self.buttonFrame = Frame(self.frame)
        self.buttonFrame.pack(side=RIGHT)

        self.button = Button(self.buttonFrame, text="QUIT", fg="red", 
                             command=self.frame.quit)
        #self.button.pack(side=LEFT)
        self.button.grid(row=0, column=0)

        #self.hi_there = Button(self.buttonFrame, 
        #text="Hello", command=self.something)
        #self.hi_there.pack(side=LEFT)
        #self.hi_there.grid(row=0, column=1)

        self.buttonClear = Button(self.buttonFrame, 
                                  text="Clear", command=self.clearText)
        #self.hi_there.pack(side=LEFT)
        self.buttonClear.grid(row=0, column=1)

        self.autoClear = IntVar()
        self.autoClear.set(1)
        self.buttonAutoClear = Checkbutton(self.buttonFrame, text="AutoClear",
                                           variable = self.autoClear)
        self.buttonAutoClear.grid(row=0, column=2)
        #self.buttonAutoClear.pack()


        self.verbose = IntVar()
        self.buttonVerbose = Checkbutton(self.buttonFrame, text="verbose",
                                         variable = self.verbose)
        self.buttonVerbose.grid(row=0, column=3)

        #Label(self.buttonFrame, 
        #      text="--- %s serial" % len(self.portList)
        #      ).grid(row=0, column=3)
              
        self.makeTextFrame()
        self.realStdout = sys.stdout
        sys.stdout = TextFrameAsFile(self)
        self.textFrameAsFile = sys.stdout

        self.widgetTable = {}
        for port in self.portList + ["all"]:
            self.widgetTable[port] = self.makeButtonForPort(port)

    def makeButtonForPort(self, port):
        if port == "all": y = 0 #len(self.portList)
        else: y = self.portList.index(port)+1

        for i in range(len(CommandList)):
            name, label, unused = CommandList[i]
            if name == None:
                b = Label(self.buttonFrame)
            else:
                b = Button(self.buttonFrame, text="[%s] " % port + label, 
                           command=(lambda name=name, port=port: 
                                    self.runModuleCommand(name, port)))
            b.grid(row = i+1, column=y)

    def makeTextFrame(self):
        # Most of this function comes from:
        #   http://python.developpez.com/faq/?page=Text#TextScrollbar
        self.textFrame = Frame(self.frame)

        s1 = Scrollbar(self.textFrame, orient=VERTICAL)
        s2 = Scrollbar(self.textFrame, orient=HORIZONTAL)
        #  t1 = Tk.Text(f1, wrap=Tk.NONE)  
        self.text = Text(self.textFrame, width=ColumnSize, height=LineSize)
        
        s1.config(command = self.text.yview)
        s2.config(command = self.text.xview)
        self.text.config(yscrollcommand = s1.set, xscrollcommand = s2.set)

        self.text.grid(column=0, row=0)
        s1.grid(column=1, row=0, sticky=S+N)
        #s2.grid(column=0, row=1, sticky=W+E)

        #self.text.pack(side=BOTTOM)
        self.textFrame.pack(side=LEFT)


    def clearText(self):
        # http://stackoverflow.com/questions/5322027/how-to-erase-everything-from-the-tkinter-text-widget
        self.text.delete("1.0", END)

    def runModuleCommand(self, command, port, checkClear = True):
        if checkClear:
            if self.autoClear.get():
                self.clearText()

        if port == "all":
            for port in self.portList:
                self.runModuleCommand(command, port, checkClear = False)
            return

        #print command, port
        for cmdName, cmdLabel, cmdArgList in CommandList:
            if cmdName == command:
                cmdArgList = cmdArgList + ["--port", "%s" % port]
                self.textFrameAsFile.ensureNewLine()
                print "*" * 10 + " (%s)" % time.asctime(),\
                    "*" * 10, command, "[%s]" % port
                print "** running:", " ".join(cmdArgList)
                self.tryRunCommand(cmdArgList)
                return
        raise ValueError("Unknown command", command)


    def tryRunCommand(self, cmdArgList):
        try:
            ModuleManagement.runAsProgram(cmdArgList, self.verbose.get())
        except:
            # http://docs.python.org/library/traceback.html
            traceback.print_exc(file=sys.stderr)
            self.textFrameAsFile.ensureNewLine()     
            formatted_lines = traceback.format_exc().splitlines()
            print "@@@EXCEPTION(see console):", formatted_lines[-1]
            

    #--------------------------------------------------

    def initModel(self):
        self.portList = ModuleManagement.getAvailablePortList()

    def something(self):
        for i in range(100):
            msg = "%s \n"%i *i # ha!
            self.text.insert("end", msg)

#---------------------------------------------------------------------------

root = Tk()
app = TkModuleManagement(root)
root.mainloop()

#---------------------------------------------------------------------------
