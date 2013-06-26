#CamIO2 dependency checker / installer

import sys
import os
import platform
import camio_prepare_arch

#list of apps/libraries required to build CamIO2
required = ["make", "clang", "configure", "svn", "cake", "bstring", "cgen", "m6" ]

#list of apps/libraries optional to build CamIO2 "exotic" transports
optional = ["netmap", "zeromq", "openssl" ]

#Main entry point
def main(install_opt):
    (find, install) = camio_prepare_arch.get_platform_defs()

    for req in required:
        if not find(req):
            install(req)

    if(install_opt):
        for opt in optional:
            if not find(opt):
                install(opt)
