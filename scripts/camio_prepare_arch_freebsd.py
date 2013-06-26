def find(dep):
    print "Not finding dependency \"" + dep + "\""

def install(dep):
    print "Not installing dependency \"" + dep + "\""

def get_defs(uname):
    print uname
    return (find,install)


