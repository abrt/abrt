from datetime import datetime

class Report():
    """Class for mapping the report to pyhon object"""
    def __init__(self, report):
        self.UUID = None
        self.Architecture = None
        self.Kernel = None
        self.Release = None
        self.Executable = None
        self.CmdLine = None
        self.Package = None
        self.TextData1 = None
        self.TextData2 = None
        self.BinaryData1 = None
        self.BinaryData2 = None
        for item in report:
            self.__dict__[item] = report[item]

    def getUUID(self):
        return self.UUID
    
    def getArchitecture(self):
        return self.Architecture
    
    def getExecutable(self):
        return self.Executable
        
    def getPackage(self):
        return self.Package
