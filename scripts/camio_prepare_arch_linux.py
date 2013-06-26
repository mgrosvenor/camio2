
def find(dep):
    print "Not finding dependency \"" + dep + "\""

def install(dep):
    print "Not installing dependency \"" + dep + "\""

def get_defs(uname, distro):
    print uname
    print distro
    return (find,install)


