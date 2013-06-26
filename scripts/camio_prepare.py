#CamIO2 dependency checker / installer

import sys
import os
import platform

macos_apps_10_8_3        = { "clang":"" }
freebsd_apps_9_1_amd64   = { "clang":"" }
ubuntu_apps_13_04_amd64  = { "clang":"" }

macos   = { "10.8.3" : macos_apps_10_8_3 }
freebsd = { "9.1_amd64" : freebsd_apps_9_1_amd64 }
ubuntu  = { "13.04_amd64" : ubuntu_apps_13_04_amd64 }

system = { "macos" : macos , "freebsd" : freebsd,  "ubuntu": ubuntu }


#Main entry point
def main():
    print "version:", platform.version()
    print "processor:", platform.processor()
    print "platform:", platform.platform()
    print "linux_dist:", platform.linux_distribution()
    print "node:", platform.node()
    print "system:", platform.system()

    print "name:", os.name    
    #print system

        
