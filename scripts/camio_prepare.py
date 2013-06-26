#CamIO2 dependency checker / installer

import sys

macos_apps_10_8_3        = { "clang":"" }
freebsd_apps_9_1_amd64   = { "clang":"" }
ubuntu_apps_13_04_amd64  = { "clang":"" }

macos   = { "10.8.3" : macos_apps_10_8_3 }
freebsd = { "9.1_amd64" : freebsd_apps_9_1_amd64 }
ubuntu  = { "13.04_amd64" : ubuntu_apps_13_04_amd64 }

system = { "macos" : macos , "freebsd" : freebsd,  "ubuntu": ubuntu }


#Main entry point
def main():
    print system

        
