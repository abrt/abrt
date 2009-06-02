from pybindgen import *
import sys
"""
 void Open(const std::string& pDir);
        void Create(const std::string& pDir);
        void Delete();
        void Close();

        bool Exist(const std::string& pFileName);

        void LoadText(const std::string& pName, std::string& pData);
        void LoadBinary(const std::string& pName, char** pData, unsigned int* pSize);

        void SaveText(const std::string& pName, const std::string& pData);
        void SaveBinary(const std::string& pName, const char* pData, const unsigned int pSize);

        void InitGetNextFile();
        bool GetNextFile(std::string& pFileName, std::string& pContent, bool& pIsTextFile);
"""
mod = Module('ABRTUtils')
mod.add_include('"../Utils/DebugDump.h"')
klass = mod.add_class('CDebugDump')
klass.add_constructor([])
klass.add_method('Create', None, [param('char*', 'pFilename')])
klass.add_method('Close', None, [])
klass.add_method('SaveText', None, [param('char*', 'pName'), param('char*', 'pData')])
mod.generate(sys.stdout)
