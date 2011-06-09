# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

from _pyreport import *


#Compatibility with report package:

import os

SYSTEM_RELEASE_PATHS = ["/etc/system-release","/etc/redhat-release"]
####SYSTEM_RELEASE_DEPS = ["system-release", "redhat-release"]

_hardcoded_default_product = ""
_hardcoded_default_version = ""

####def getProduct_fromPRODUCT():
####    try:
####        import product
####        return product.productName
####    except:
####        return ""

####def getVersion_fromPRODUCT():
####    try:
####        import product
####        return product.productVersion
####    except:
####        return ""

####def getProduct_fromRPM():
####    try:
####        import rpm
####        ts = rpm.TransactionSet()
####        for each_dep in SYSTEM_RELEASE_DEPS:
####            mi = ts.dbMatch('provides', each_dep)
####            for h in mi:
####                if h['name']:
####                    return h['name'].split("-")[0].capitalize()
####
####        return ""
####    except:
####        return ""

####def getVersion_fromRPM():
####    try:
####        import rpm
####        ts = rpm.TransactionSet()
####        for each_dep in SYSTEM_RELEASE_DEPS:
####            mi = ts.dbMatch('provides', each_dep)
####            for h in mi:
####                if h['version']:
####                    return str(h['version'])
####        return ""
####    except:
####        return ""

def getProduct_fromFILE():
    for each_path in SYSTEM_RELEASE_PATHS:
        try:
            file = open(each_path, "r")
            content = file.read()
            if content.startswith("Red Hat Enterprise Linux"):
                return "Red Hat Enterprise Linux"
            if content.startswith("Fedora"):
                return "Fedora"
            i = content.find(" release")
            if i > -1:
                return content[0:i]
        except:
            pass
    return ""

def getVersion_fromFILE():
    for each_path in SYSTEM_RELEASE_PATHS:
        try:
            file = open(each_path, "r")
            content = file.read()
            if content.find("Rawhide") > -1:
                return "rawhide"
            clist = content.split(" ")
            i = clist.index("release")
            return clist[i+1]
        except:
            pass
    return ""

def getProduct():
    ####product = getProduct_fromPRODUCT()
    ####if product:
    ####    return product
    product = getProduct_fromFILE()
    if product:
        return product
    ####product = getProduct_fromRPM()
    ####if product:
    ####    return product
    return _hardcoded_default_product

def getVersion():
    ####version = getVersion_fromPRODUCT()
    ####if version:
    ####    return version
    version = getVersion_fromFILE()
    if version:
        return version
    ####version = getVersion_fromRPM()
    ####if version:
    ####    return version
    return _hardcoded_default_version

def createAlertSignature(component, hashmarkername, hashvalue, summary, alertSignature):
    pd = problem_data()
    pd.add("component", component)
    pd.add("hashmarkername", hashmarkername)
    pd.add("duphash", hashvalue)
    pd.add("reason", summary)
    pd.add("description", alertSignature)
    pd.add_basics()

    return pd

# used in anaconda / python-meh
def createPythonUnhandledExceptionSignature(component, hashmarkername, hashvalue, summary, description, exnFileName):
    pd = problem_data()
    pd.add("component", component)
    pd.add("hashmarkername", hashmarkername)
    #cd.add("localhash", hashvalue)
    pd.add("duphash", hashvalue)
    pd.add("reason", summary)
    pd.add("description", description)
    #pd.add("product", getProduct())
    #pd.add("version", getVersion())
    pd.add_basics() # adds product and version + some other required field
    # FIXME: how to handle files out of dump dir??
    #1 = flag BIN
    pd.add("pythonUnhandledException", exnFileName, 1)

    return pd

"""
def report(cd, io_unused):
    state = run_event_state()
    #state.logging_callback = logfunc
    r = state.run_event_on_problem_data(cd, "report")
    return r
"""

def report(pd, io_unused):
    result = report_problem(pd)
