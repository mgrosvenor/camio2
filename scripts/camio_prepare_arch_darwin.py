macos_apps_10_8_3        = { "clang":"" }
freebsd_apps_9_1_amd64   = { "clang":"" }
ubuntu_apps_13_04_amd64  = { "clang":"" }

darwin   = { "10.8.3" : macos_apps_10_8_3 }
freebsd = { "9.1_amd64" : freebsd_apps_9_1_amd64 }
ubuntu  = { "13.04_amd64" : ubuntu_apps_13_04_amd64 }

system = { "darwin" : darwin , "freebsd" : freebsd,  "ubuntu": ubuntu }

def find(dep):
    print "Not finding dependency \"" + dep + "\""

def install(dep):
    print "Not installing dependency \"" + dep + "\""


def get_defs(uname,mac_ver):
    print uname
    print mac_ver
    return (find,install)


