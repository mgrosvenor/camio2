import camio_prepare_repo_deps
import subprocess
import sys


#Translation from local package name to the name used by RPM
rpm_deps    = {
                "make":  "make",
                "clang": "clang",
                "svn":   "subversion",
                "wget":  "wget",
                "unzip" : "unzip",
                "yum" :  "yum",
              }


#Translation from local package name to the name used by APT
apt_deps    = {
                "make":  "make",
                "clang": "clang",
                "svn":   "subversion",
                "apt":   "apt",
                "wget":  "wget",
                "unzip" : "unzip",
              } 


#Tools for handling the repo deps, these are assumed to be global across plaforms
find_repo_deps    = camio_prepare_repo_deps.find_repo_deps
install_repo_deps = camio_prepare_repo_deps.install_repo_deps 

############################################################################################################################
#Use apt tools to manage packages

def check_dpkg():
    cmd = "dpkg --version"
    result = subprocess.call(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result:
        print "CamIO2 Prepare: Could not find \"dpkg\" which is necessary for running CamIO2 Prepare on Ubuntu" 
        print "CamIO2 Prepare: Fatal Error! Exiting now."
        sys.exit(1)
    return True


def find_apt(dep):
    #This one is special since we use it to figure out the rest...
    if dep == "dpkg":
        return check_dpkg

    if dep in apt_deps:
        cmd = "dpkg -s " + apt_deps[dep] + " | grep \"Status: install ok installed\""
        result = subprocess.call(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return not result #subprocess returns 0 on sucess, 1 on failure

    result = find_repo_deps(dep)
    if result != None:
        return result
    
    print "\nCamIO2 Prepare: The depency \"" + dep + "\" cannot be resolved.\n"\
          "                The dependency does not exist in the list of known apt or repository based resources.\n"\
          "                This is a CamIO2 Preapre internal error."
    return None


def do_install_apt(dep):
    if dep in apt_deps:
        depapt = apt_deps[dep]
        cmd = "sudo apt-get -y install " + depapt 
        print "                Running \"" + cmd + "\""
        result = subprocess.call(cmd, shell=True)
        return not result #subprocess returns 0 on sucess, 1 on failure

    result = install_repo_deps(dep, "wget" )
    if result != None:
        return result
 
    print "\nCamIO2 Prepare: The depency \"" + dep + "\" cannot be resolved.\n"\
          "                The dependency does not exist in the list of known apt repository based resources.\n"\
          "                Installation failed.\n"\
          "                This is a CamIO2 Preapre internal error."



def install_apt(required, optional):

    #Figure out what needs to be installed and do it
    for req in required:
        sys.stdout.write("CamIO2 Prepare: Looking for " + req + "...")
        result = find_apt(req)
        if result == None:
            print "CamIO2 Prepare: Fatal Error! Exiting now."
            sys.exit(-1)

        #The dependency exists
        if result:
            sys.stdout.write(" Found.\n")
            continue

        #The dependency needs installing
        sys.stdout.write(" Installing...\n")
        if not  do_install_apt(req):
            print "CamIO2 Prepare: Could not install dependency \"" + req + "\""
            print "CamIO2 Preapre: Fatal Error! Exiting now."
            sys.exit(-1)

   
############################################################################################################################
#Use RPM tools to manage packages


def find_rpm(dep):
    print "CamIO2 Prepare: find_rpm not implemented"
    print "CamIO2 Prepare: Fatal Error. Exiting now."
    sys.exit(-1)

    
def do_install_rpm(dep):
    print "CamIO2 Prepare: do_install_rpm ot implemented"
    print "CamIO2 Prepare: Fatal Error. Exiting now."
    sys.exit(-1)

def install_rpm(required, optional):
    print "CamIO2 Prepare: install_rpm ot implemented"
    print "CamIO2 Prepare: Fatal Error. Exiting now."
    sys.exit(-1)



############################################################################################################################

#Handle Ubuntu Distribution

def install_ubuntu(required, optional):
    return install_apt(required, optional)    

############################################################################################################################

#Handle Debian Distribution

def install_debian(required, optional):
    return install_apt(required, optional)    


############################################################################################################################

#Handle Fedora Distribution

def install_fedora(required, optional):
    return install_rpm(required, optional)

############################################################################################################################

#Figure out which distribution we're running and use the appropriate find/install functions 
def install(uname, distro, required_arch, required, optional):   
    
    (distname, ver, ver_name) = distro
    required = required_arch[distname] + required
    
    if distname == "Ubuntu":
        print "CamIO2 Prepare: Detcted system is running \"" + distname + "\""
        return install_ubuntu(required, optional)

    if distname == "debian":
        print "CamIO2 Prepare: Warning! Untested Distribution"
        print "CamIO2 Prepare: Detcted system is running \"" + distname + "\""
        return install_debian(required, optional)

    if distname == "Fedora":
        print "CamIO2 Prepare: Warning! Untested Distribution"
        print "CamIO2 Prepare: Detcted system is running \"" + distname + "\""
        return install_fedora( required, optional)


    print "CamIO2 Prepare: Unknown Linux distribution \"" + distname + "\""
    print "CamIO2 Prepare: Fatal Error! Exiting now."
    sys.exit(-1)
    
