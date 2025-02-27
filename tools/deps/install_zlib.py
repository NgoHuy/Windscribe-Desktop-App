#!/usr/bin/env python
# ------------------------------------------------------------------------------
# Windscribe Build System
# Copyright (c) 2020-2023, Windscribe Limited. All rights reserved.
# ------------------------------------------------------------------------------
# Purpose: installs ZLib library.
import os
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS_DIR)

CONFIG_NAME = os.path.join("vars", "zlib.yml")

# To ensure modules in the 'base' folder can import other modules in base.
import base.pathhelper as pathhelper
sys.path.append(pathhelper.BASE_DIR)

import base.messages as msg
import base.utils as utl
import installutils as iutl

# Dependency-specific settings.
DEP_TITLE = "ZLib"
DEP_URL = "https://github.com/madler/zlib/archive/"
DEP_OS_LIST = ["win32"]
DEP_FILE_MASK = ["include/*", "lib/*"]


def BuildDependencyMSVC(outpath):
    # Create an environment with VS vars.
    buildenv = os.environ.copy()
    buildenv.update({"MAKEFLAGS": "S"})
    buildenv.update(iutl.GetVisualStudioEnvironment(is_arm64_build))

    # Build.
    build_cmd = iutl.GetMakeBuildCommand()
    build_cmd.extend(["/F", "win32/Makefile.msc"])
    iutl.RunCommand(build_cmd, env=buildenv, shell=True)
    currend_wd = os.getcwd()
    utl.CreateDirectory("{}/include".format(outpath), True)
    utl.CopyFile("{}/zconf.h".format(currend_wd), "{}/include/zconf.h".format(outpath))
    utl.CopyFile("{}/zlib.h".format(currend_wd), "{}/include/zlib.h".format(outpath))
    utl.CreateDirectory("{}/lib".format(outpath), True)
    utl.CopyFile("{}/zdll.exp".format(currend_wd), "{}/lib/zdll.exp".format(outpath))
    utl.CopyFile("{}/zdll.lib".format(currend_wd), "{}/lib/zdll.lib".format(outpath))
    utl.CopyFile("{}/zlib1.dll".format(currend_wd), "{}/lib/zlib1.dll".format(outpath))


def InstallDependency():
    # Load environment.
    msg.HeadPrint("Loading: \"{}\"".format(CONFIG_NAME))
    configdata = utl.LoadConfig(os.path.join(TOOLS_DIR, CONFIG_NAME))
    if not configdata:
        raise iutl.InstallError("Failed to get config data.")
    iutl.SetupEnvironment(configdata)
    dep_name = DEP_TITLE.lower()
    dep_version_var = "VERSION_" + DEP_TITLE.upper().replace("-", "")
    dep_version_str = os.environ.get(dep_version_var, None)
    if not dep_version_str:
        raise iutl.InstallError("{} not defined.".format(dep_version_var))
    # Prepare output.
    temp_dir = iutl.PrepareTempDirectory(dep_name)
    # Download and unpack the archive.
    archivetitle = "{}-{}".format(dep_name, dep_version_str)
    archivename = "v{}.zip".format(dep_version_str)
    localfilename = os.path.join(temp_dir, archivename)
    msg.HeadPrint("Downloading: \"{}\"".format(archivename))
    iutl.DownloadFile("{}{}".format(DEP_URL, archivename), localfilename)
    msg.HeadPrint("Extracting: \"{}\"".format(archivename))
    iutl.ExtractFile(localfilename)
    # Build the dependency.
    dep_buildroot_str = os.path.join("build-libs-arm64" if is_arm64_build else "build-libs", dep_name)
    outpath = os.path.normpath(os.path.join(os.path.dirname(TOOLS_DIR), dep_buildroot_str))
    with utl.PushDir(os.path.join(temp_dir, archivetitle)):
        msg.HeadPrint("Building: \"{}\"".format(archivetitle))
        BuildDependencyMSVC(temp_dir)
    # Copy the dependency to output directory and to a zip file, if needed.
    installzipname = None
    if "-zip" in sys.argv:
        dep_artifact_var = "ARTIFACT_" + DEP_TITLE.upper()
        dep_artifact_str = os.environ.get(dep_artifact_var, "{}.zip".format(dep_name))
        installzipname = os.path.join(os.path.dirname(outpath), dep_artifact_str)
    msg.Print("Installing artifacts...")
    aflist = iutl.InstallArtifacts(temp_dir, DEP_FILE_MASK, outpath, installzipname)
    for af in aflist:
        msg.HeadPrint("Ready: \"{}\"".format(af))
    # Cleanup.
    msg.Print("Cleaning temporary directory...")
    utl.RemoveDirectory(temp_dir)
    msg.Print(f"{DEP_TITLE} v{dep_version_str} installed in {outpath}")


if __name__ == "__main__":
    start_time = time.time()
    current_os = utl.GetCurrentOS()
    is_arm64_build = "--arm64" in sys.argv
    if is_arm64_build:
        msg.Print(f"Cross compiling arm64 on Windows not currently implemented for {DEP_TITLE}.")
        sys.exit(0)
    if current_os not in DEP_OS_LIST:
        msg.Print("{} is not needed on {}, skipping.".format(DEP_TITLE, current_os))
        sys.exit(0)
    try:
        msg.Print("Installing {}...".format(DEP_TITLE))
        InstallDependency()
        exitcode = 0
    except iutl.InstallError as e:
        msg.Error(e)
        exitcode = e.exitcode
    except IOError as e:
        msg.Error(e)
        exitcode = 1
    elapsed_time = time.time() - start_time
    if elapsed_time >= 60:
        msg.HeadPrint("All done: %i minutes %i seconds elapsed" % (elapsed_time / 60, elapsed_time % 60))
    else:
        msg.HeadPrint("All done: %i seconds elapsed" % elapsed_time)
    sys.exit(exitcode)
