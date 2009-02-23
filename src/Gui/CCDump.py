from datetime import datetime

class Dump():
    """Class for mapping the debug dump to pyhon object"""
    def __init__(self):
        self.UUID = None
        self.UID = None
        self.Count = None
        self.Executable = None
        self.Package = None
        self.Time = None
    
    def getUUID(self):
        return self.UUID
    
    def getUID(self):
        return self.UID
        
    def getCount(self):
        return self.Count
    
    def getExecutable(self):
        return self.Executable
        
    def getPackage(self):
        return self.Package
    
    def getTime(self,format):
        #print format
        if format:
            try:
                return datetime.fromtimestamp(int(self.Time)).strftime(format)
            except Exception, e:
                print e
        return int(self.Time)
