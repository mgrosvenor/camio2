import sys

req_version = (2,5)
cur_version = sys.version_info

if cur_version >= req_version:
    import camio_install
    camio_install.run()
else:
    print "Your Python interpreter is older than version 2.5. CamIO requires version 2.5 or later. Please upgrade if you'd like to continue using CamIO"


def run():
    print "ha ha ha"
        
