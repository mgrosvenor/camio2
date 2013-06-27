import os
import subprocess
import shutil

#Since we fetch the source code from source code repositories, these are assumed to build for all platforms
#if this is not the case, platform specific repos will need to be fetched from the platform specific arch file. 

#Translation from local packge name to the (svn path, install path, build command)
svn_deps    = { 
                "libcgen"    : ("http://c-generic-library.googlecode.com/svn/trunk/", "deps/libcgen", None),
              } 
                    
#Translation from the local package name to the (git path, install path, build command)
git_deps    = { 
                "cake"  : ("https://github.com/Zomojo/Cake.git", "deps/cake", None),
                "libm6" : ("https://github.com/mgrosvenor/libm6.git", "deps/libm6", None),
              } 

#translation from local name to the cmd line to fetch and unpack (presumably with wget), the install path, the build command
url_deps   = {
                "libbstring" : ("http://sourceforge.net/projects/bstring/files/bstring/rc31/bstrlib-05122010.zip/download", "deps/libbstring", "unzip ../deps/libbstring/libbstring -d ../deps/libbstring"), 
             } 


def install_dep_svn(dep):
    svn_str   = svn_deps[dep][0]
    path_str  = "../" + svn_deps[dep][1]
    build_str = svn_deps[dep][2]

    os.makedirs(path_str) 
    cmd = "svn checkout " + svn_str + " " + path_str
    print "CamIO2 Prepare: Running " + cmd
    result = subprocess.call(cmd, shell=True)
    if result:
        shutil.rmtree(path_str)
        print "CamIO2 Prepare: Could not install \"" + dep + "\" using svn"
    
    return not result


def install_dep_git(dep):
    git_str   = git_deps[dep][0]
    path_str  = "../" + git_deps[dep][1]
    build_str = git_deps[dep][2]

    os.makedirs(path_str) 
    cmd = "git clone " + git_str + " " + path_str
    print "CamIO2 Prepare: Running " + cmd
    result = subprocess.call(cmd, shell=True)
    if result:
        shutil.rmtree(path_str)
        print "CamIO2 Prepare: Could not install \"" + dep  + "\" using git"
    
    return not result


def install_dep_url(dep, fetch_tool):
    print "Installing " + dep + " using " + fetch_tool

    url_str   = url_deps[dep][0]
    path_str  = "../" + url_deps[dep][1]
    unpack_str = url_deps[dep][2]

    os.makedirs(path_str) 


    if fetch_tool == "wget":
        cmd = "wget " + "-O " + path_str + "/" + dep + " " + url_str
    else: 
        print "CamIO2 Prepare: Unknown fetch tool \"" + fetch_tool + "\""
        print "CamIO2 Prepare: Fatal Error. Exiting now."
        sys.exit(-1)

    print "CamIO2 Prepare: Running " + cmd
    result = subprocess.call(cmd, shell=True)
    if result:
        shutil.rmtree(path_str)
        print "CamIO2 Prepare: Could not get  \"" + dep  + "\" using " + fetch_tool
        return False
    

    cmd = unpack_str
    print "CamIO2 Prepare: Running " + cmd
    result = subprocess.call(cmd, shell=True)
    if result:
        #shutil.rmtree(path_str)
        print "CamIO2 Prepare: Could not unpack  \"" + dep  + "\" using " + cmd
    
    return not result




def find_repo_deps(dep):
    path = None
    
    if dep in svn_deps:       
        path = svn_deps[dep][1]
    elif dep in git_deps:
        path = git_deps[dep][1]
    elif dep in url_deps:
        path = url_deps[dep][1]
    
    if path :
        return os.path.exists("../" + path)
    
    return None



def install_repo_deps(dep, fetch_tool):

    if dep in svn_deps:       
        return install_dep_svn(dep)
    elif dep in git_deps:
        return install_dep_git(dep)
    elif dep in url_deps:
        return install_dep_url(dep, fetch_tool) 

    return None


