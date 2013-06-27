#CamIO2 dependency checker / installer

import sys
import os
import platform
import camio_prepare_arch


#list of paths required to exist
build_paths = ["deps"]

#list of apps/libraries required to build CamIO2
required = [ "unzip", "make", "clang", "svn", "cake", "libbstring", "libcgen", "libm6" ]

#List of arch specific apps required
required_linux_ubuntu = [ "dpkg", "apt", "wget" ]
required_linux_debian = [ "dpkg", "apt", "wget" ]
required_linux_fedora = [ "rpm", "yum", "wget" ]
required_darwin = [ "curl" ] 
required_freebsd = [ "pkg_add", "fetch" ] 

required_arch = { 
                    "Ubuntu"  : required_linux_ubuntu,
                    "debian"  : required_linux_debian,
                    "Fedora"  : required_linux_fedora,
                    "Darwin"  : required_darwin,
                    "FreeBSD" : required_freebsd,
                }

#list of apps/libraries optional to build CamIO2 "exotic" transports
optional = ["libnetmap", "libzeromq", "libopenssl" ]

#Main entry point
def main(install_opt):

    #Make sure all the development paths are ready
    for path in build_paths:
        path = "../" + path #We're running inside of the scripts dir
        if not os.path.exists(path):
            print "CamIO2 Prepare: Making build path \"" + path + "\"..."
            os.mkdir(path)

    #Run the platform specific installer
    camio_prepare_arch.platform_install( required_arch, required, optional)
   
