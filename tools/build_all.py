#!/usr/bin/env python
# ------------------------------------------------------------------------------
# Windscribe Build System
# Copyright (c) 2020-2023, Windscribe Limited. All rights reserved.
# ------------------------------------------------------------------------------
# Purpose: builds Windscribe.
import glob
import os
import pathlib
import platform
import re
import subprocess
import sys
import time
import zipfile
import multiprocessing

from pathlib import Path

# To ensure modules in the 'base' folder can import other modules in base.
import base.pathhelper as pathhelper
sys.path.append(pathhelper.BASE_DIR)

import base.messages as msg
import base.process as proc
import base.utils as utl
import base.extract as extract
import base.secrethelper as secrethelper
import deps.installutils as iutl
from base.arghelper import ArgHelper

# Windscribe settings.
BUILD_TITLE = "Windscribe"
BUILD_CFG_NAME = "build_all.yml"
BUILD_OS_LIST = ["win32", "macos", "linux"]

CURRENT_OS = ""

BUILD_MAC_DEPLOY = ""
BUILD_INSTALLER_FILES = ""
BUILD_SYMBOL_FILES = ""
MAC_DEV_ID_KEY_NAME = ""
MAC_DEV_TEAM_ID = ""
arghelper = ArgHelper(sys.argv)
extractor = extract.Extractor()


def remove_files(files):
    for f in files:
        try:
            if os.path.isfile(f):
                os.remove(f)
        except OSError as err:
            print("Error: %s : %s" % (f, err.strerror))


def remove_empty_dirs(dirs):
    for d in dirs:
        try:
            if len(os.listdir(d)) == 0 and os.path.isdir(d):
                os.rmdir(d)
            else:
                subdirs = []
                for subd in os.listdir(d):
                    subdirs.append(os.path.join(d, subd))
                remove_empty_dirs(subdirs)
                os.rmdir(d)
        except OSError as err:
            print("Error: %s : %s" % (d, err.strerror))


def delete_all_files(root_dir, dir_pattern):
    dirs_to_clean = glob.glob(root_dir + os.sep + dir_pattern + "*")
    for directory in dirs_to_clean:
        remove_files(glob.glob(directory + "*/**/*", recursive=True))
        remove_files(glob.glob(directory + "*/**/.*", recursive=True))
    remove_empty_dirs(dirs_to_clean)


def clean_all_temp_and_build_dirs():
    delete_all_files(pathhelper.ROOT_DIR, "build")
    delete_all_files(pathhelper.ROOT_DIR, "build-exe")
    delete_all_files(pathhelper.ROOT_DIR, "temp")


def generate_include_file_from_pub_key(key_filename_absolute, pubkey):
    with open(pubkey, "r") as infile:
        with open(key_filename_absolute, "w") as outfile:
            outfile.write("R\"(")
            for line in infile:
                outfile.write(line)
            outfile.write(")\"")


def update_version_in_plist(plistfilename):
    with open(plistfilename, "r") as f:
        file_data = f.read()
    # update Bundle Version
    file_data = re.sub("<key>CFBundleVersion</key>\n[^\n]+",
                       "<key>CFBundleVersion</key>\n\t<string>{}</string>"
                       .format(extractor.app_version()),
                       file_data, flags=re.M)
    # update Bundle Version (short)
    file_data = re.sub("<key>CFBundleShortVersionString</key>\n[^\n]+",
                       "<key>CFBundleShortVersionString</key>\n\t<string>{}</string>"
                       .format(extractor.app_version()),
                       file_data, flags=re.M)
    with open(plistfilename, "w") as f:
        f.write(file_data)


def update_version_in_config(filename):
    with open(filename, "r") as f:
        filedata = f.read()
    # update Bundle Version
    filedata = re.sub("Version:[^\n]+",
                      "Version: {}".format(extractor.app_version()),
                      filedata, flags=re.M)
    with open(filename, "w") as f:
        f.write(filedata)


def update_arch_in_config(filename):
    with open(filename, "r") as f:
        filedata = f.read()
    # update Bundle Version
    if platform.processor() == "x86_64":
        filedata = re.sub("Architecture:[^\n]+", "Architecture: amd64", filedata, flags=re.M)
    elif platform.processor() == "aarch64":
        filedata = re.sub("Architecture:[^\n]+", "Architecture: arm64", filedata, flags=re.M)
    with open(filename, "w") as f:
        f.write(filedata)


def update_team_id(filename):
    with open(filename, "r") as f:
        filedata = f.read()
    outdata = filedata.replace("$(DEVELOPMENT_TEAM)", MAC_DEV_TEAM_ID)
    with open(filename, "w") as f:
        f.write(outdata)


def restore_helper_info_plist(filename):
    with open(filename, "r") as f:
        filedata = f.read()
    outdata = filedata.replace(MAC_DEV_TEAM_ID, "$(DEVELOPMENT_TEAM)")
    with open(filename, "w") as f:
        f.write(outdata)


def get_project_file(subdir_name, project_name):
    return os.path.normpath(os.path.join(pathhelper.ROOT_DIR, subdir_name, project_name))


def get_project_folder(subdir_name):
    return os.path.normpath(os.path.join(pathhelper.ROOT_DIR, subdir_name))


def copy_file(filename, srcdir, dstdir, strip_first_dir=False):
    parts = filename.split("->")
    srcfilename = parts[0].strip()
    dstfilename = srcfilename if len(parts) == 1 else parts[1].strip()
    msg.Print(dstfilename)
    srcfile = os.path.normpath(os.path.join(srcdir, srcfilename))
    dstfile = os.path.normpath(dstfilename)
    if strip_first_dir:
        dstfile = os.sep.join(dstfile.split(os.path.sep)[1:])
    dstfile = os.path.join(dstdir, dstfile)
    utl.CopyAllFiles(srcfile, dstfile) \
        if srcfilename.endswith(("\\", "/")) else utl.CopyFile(srcfile, dstfile)


def copy_files(title, filelist, srcdir, dstdir, strip_first_dir=False):
    msg.Info("Copying {} files...".format(title))
    for filename in filelist:
        copy_file(filename, srcdir, dstdir, strip_first_dir)


def fix_rpath_linux(filename):
    parts = filename.split("->")
    srcfilename = parts[0].strip()
    rpath = "" if len(parts) == 1 else parts[1].strip()
    iutl.RunCommand(["patchelf", "--set-rpath", rpath, srcfilename])


def fix_rpath_macos(filename):
    dylib_name = os.path.basename(filename)
    pipe = subprocess.Popen(['otool', '-L', filename], stdout=subprocess.PIPE)
    while True:
        line = pipe.stdout.readline().decode("utf-8").strip()
        if line == '':
            break
        if "/build-libs/" in line and "(compatibility version" in line:
            old_dylib_path = line[:line.find("(compatibility version")].strip()
            old_dylib_root = old_dylib_path[:old_dylib_path.find("/build-libs/")]
            new_dylib_root = filename[:filename.find("/build-libs/")]
            new_dylib_path = old_dylib_path.replace(old_dylib_root, new_dylib_root)
            if new_dylib_path != old_dylib_path:
                msg.HeadPrint("Patching {}: {} -> {}".format(dylib_name, old_dylib_path, new_dylib_path))
                if os.path.basename(old_dylib_path) == dylib_name:
                    iutl.RunCommand(["install_name_tool", "-id", new_dylib_path, filename])
                else:
                    iutl.RunCommand(["install_name_tool", "-change", old_dylib_path, new_dylib_path, filename])


def fix_build_libs_rpaths(configdata):
    # The build-libs are downloaded and extracted from zips to some arbitrary build path on the
    # developer/CI machine.  The rpaths stamped into the macOS dylibs and linux shared objects
    # when they were built will contain the full path of the machine used to create them.  We
    # need to change these rpaths to those of the machine this script is now running on.
    if CURRENT_OS == "macos":
        if "rpath_fix_files_mac" in configdata:
            for build_lib_name, binaries_to_patch in configdata["rpath_fix_files_mac"].items():
                build_lib_root = iutl.GetDependencyBuildRoot(build_lib_name)
                if not os.path.exists(build_lib_root):
                    raise iutl.InstallError("Cannot fix {} rpath, installation not found at {}".format(build_lib_name, build_lib_root))
                for binary_name in binaries_to_patch:
                    fix_rpath_macos(os.path.join(build_lib_root, binary_name))


def apply_mac_deploy_fixes(configdata, target, appname, fullpath):
    # Special deploy fixes for Mac.
    # 1. copy_libs
    copy_libs(configdata, "macos", fullpath)
    # 2. remove_files
    if "remove" in configdata["client_deploy_files"]["macos"]:
        msg.Info("Removing unnecessary files...")
        for k in configdata["client_deploy_files"]["macos"]["remove"]:
            utl.RemoveFile(os.path.join(appname, k))
    # 3. rpathfix
    if "fix_rpath" in configdata["client_deploy_files"]["macos"]:
        with utl.PushDir():
            msg.Info("Fixing rpaths...")
            for f, m in configdata["client_deploy_files"]["macos"]["fix_rpath"].items():
                fs = os.path.split(f)
                os.chdir(os.path.join(fullpath, fs[0]))
                for k, v in m.items():
                    lib_root = iutl.GetDependencyBuildRoot(k)
                    if not lib_root:
                        raise iutl.InstallError("Library \"{}\" is not installed.".format(k))
                    for vv in v:
                        parts = vv.split("->")
                        srcv = parts[0].strip()
                        dstv = srcv if len(parts) == 1 else parts[1].strip()
                        change_lib_from = os.path.join(lib_root, srcv)
                        change_lib_to = "@executable_path/{}".format(dstv)
                        # Ensure the lib actually exists (i.e. we have the correct name in build_all.yml)
                        if not os.path.exists(change_lib_from):
                            raise IOError("Cannot find file \"{}\"".format(change_lib_from))
                        iutl.RunCommand(["install_name_tool", "-change", change_lib_from, change_lib_to, fs[1]])
    # 4. Code signing.
    # The Mac app must be signed in order to install and operate properly.
    msg.Info("Signing the app bundle...")
    iutl.RunCommand(["codesign", "--deep", appname, "--options", "runtime", "--timestamp", "-s", MAC_DEV_ID_KEY_NAME, "-f"])
    # This validation is optional.
    iutl.RunCommand(["codesign", "-v", appname])
    if "entitlements" in configdata["client_deploy_files"]["macos"] \
            and "entitlements_binary" in configdata["client_deploy_files"]["macos"]["entitlements"] \
            and "entitlements_file" in configdata["client_deploy_files"]["macos"]["entitlements"]:
        # Can only sign with entitlements if the embedded provisioning file exists.  The client will segfault on
        # launch otherwise with a "EXC_CRASH (Code Signature Invalid)" exception type.
        if os.path.exists(pathhelper.mac_provision_profile_filename_absolute()):
            msg.Info("Signing a binary with entitlements...")
            entitlements_binary = os.path.join(appname, configdata["client_deploy_files"]["macos"]["entitlements"]["entitlements_binary"])
            entitlements_file = os.path.join(pathhelper.ROOT_DIR, configdata["client_deploy_files"]["macos"]["entitlements"]["entitlements_file"])
            entitlements_file_temp = entitlements_file + "_temp"
            utl.CopyFile(entitlements_file, entitlements_file_temp)
            update_team_id(entitlements_file_temp)
            iutl.RunCommand(["codesign", "--entitlements", entitlements_file_temp, "-f",
                             "-s", MAC_DEV_ID_KEY_NAME, "--options", "runtime", "--timestamp",
                             entitlements_binary])
            utl.RemoveFile(entitlements_file_temp)
        else:
            msg.Warn("No embedded.provisionprofile found for this project.  IKEv2 will not function in this build.")


def build_component(component, qt_root, buildenv=None, force_rebuild=False):
    msg.Info("Building {}...".format(component["name"]))
    with utl.PushDir() as current_wd:
        temp_wd = os.path.normpath(os.path.join(current_wd, component["subdir"]))
        if force_rebuild:
            utl.RemoveDirectory(temp_wd)
        utl.CreateDirectory(temp_wd)
        os.chdir(temp_wd)
        if CURRENT_OS == "macos" and component["name"] == "Helper":
            # Update the team ID in the helper's plist.  xcodebuild won't do it for us as we are embedding the
            # plist via the Other Linker Flags section of the Xcode project.
            update_team_id(os.path.join(pathhelper.ROOT_DIR, component["subdir"], "helper-info.plist"))

        generate_cmd = ["cmake", f"-DCMAKE_PREFIX_PATH:PATH={qt_root}", os.path.join(pathhelper.ROOT_DIR, component["subdir"], "CMakeLists.txt")]
        generate_cmd.extend(["--no-warn-unused-cli", "-DCMAKE_BUILD_TYPE=" + ("Debug" if arghelper.build_debug() else "Release")])
        if arghelper.sign_app():
            generate_cmd.extend(["-DDEFINE_USE_SIGNATURE_CHECK_MACRO=ON"])
        if CURRENT_OS == "macos":
            generate_cmd.extend(["-DCMAKE_OSX_ARCHITECTURES=\'arm64;x86_64\'"])
        if "generator" in component:
            generate_cmd.extend(["-G", component["generator"]])
        elif CURRENT_OS == "win32":
            generate_cmd.append('-G Ninja')
            generate_cmd.append("-DCMAKE_GENERATOR:STRING=Ninja")
        if arghelper.target_arm64_arch() and CURRENT_OS == "win32":
            generate_cmd.append("-DCMAKE_SYSTEM_NAME:STRING=Windows")
            generate_cmd.append("-DCMAKE_SYSTEM_PROCESSOR:STRING=arm64")
            generate_cmd.append("-DCMAKE_SYSTEM_VERSION:STRING=10")
            generate_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE:FILEPATH={qt_root}/lib/cmake/Qt6/qt.toolchain.cmake")
            if arghelper.ci_mode():
                generate_cmd.append("-DQT_HOST_PATH={}".format(os.path.join(pathhelper.ROOT_DIR, "build-libs", "qt")))

        if component["name"] == "Client":
            try:
                build_id = re.search(r"\d+", proc.ExecuteAndGetOutput(["git", "branch", "--show-current"], env=buildenv, shell=False)).group()
                generate_cmd.extend(["-DDEFINE_USE_BUILD_ID_MACRO=" + build_id])
            except Exception:
                # Not on a development branch, ignore
                pass
        msg.Info(generate_cmd)
        iutl.RunCommand(generate_cmd, env=buildenv, shell=(CURRENT_OS == "win32"))

        build_cmd = ["cmake", "--build", ".", "--config", "Debug" if arghelper.build_debug() else "Release", "--parallel", str(multiprocessing.cpu_count()), "--"]
        if "xcflags" in component:
            build_cmd.extend(component["xcflags"])
        if CURRENT_OS == "macos":
            build_cmd.extend(["DEVELOPMENT_TEAM={}".format(MAC_DEV_TEAM_ID)])
            if component["name"] == "Installer":
                utl.CopyFile(os.path.join(pathhelper.ROOT_DIR, component["subdir"], "Info.plist"),
                             os.path.join(pathhelper.ROOT_DIR, component["subdir"], "temp_Info.plist"))
                temp_info_plist = os.path.join(pathhelper.ROOT_DIR, component["subdir"], "temp_Info.plist")
                update_version_in_plist(temp_info_plist)
                build_cmd.extend(["INFOPLIST_FILE={}".format(temp_info_plist)])

        msg.Info(build_cmd)
        iutl.RunCommand(build_cmd, env=buildenv, shell=(CURRENT_OS == "win32"))

        if CURRENT_OS == "macos":
            if component["name"] == "Helper":
                # Undo what UpdateTeamID did above so version control doesn't see the change.
                restore_helper_info_plist(os.path.join(pathhelper.ROOT_DIR, component["subdir"], "helper-info.plist"))
            elif component["name"] == "Installer":
                utl.RemoveFile(temp_info_plist)


def deploy_component(configdata, component_name, buildenv=None, target_name_override=None):
    component = configdata[component_name][CURRENT_OS]
    c_target = component.get("target", None)

    with utl.PushDir() as current_wd:
        temp_wd = os.path.normpath(os.path.join(current_wd, component["subdir"]))
        os.chdir(temp_wd)

        if CURRENT_OS == "macos" and "deploy" in component and component["deploy"]:
            deploy_cmd = [BUILD_MAC_DEPLOY, c_target]
            if "plugins" in component and not component["plugins"]:
                deploy_cmd.append("-no-plugins")
            iutl.RunCommand(deploy_cmd, env=buildenv)
            update_version_in_plist(os.path.join(temp_wd, c_target, "Contents", "Info.plist"))
            if component["name"] == "Client":
                # Could not find an automated way to do this like we could with xcodebuild.
                update_team_id(os.path.join(temp_wd, c_target, "Contents", "Info.plist"))

        if CURRENT_OS == "macos" and component["name"] in ["CLI", "Client"] or CURRENT_OS == "linux" or CURRENT_OS == "win32":
            target_location = ""
        elif arghelper.build_debug():
            target_location = "Debug"
        else:
            target_location = "Release"

        # Apply Mac deploy fixes to the app.
        if CURRENT_OS == "macos" and "deploy" in component and component["deploy"]:
            appfullname = os.path.join(temp_wd, target_location, c_target)
            apply_mac_deploy_fixes(configdata, component_name, c_target, appfullname)

        # Copy output file(s).
        if c_target:
            targets = [c_target] if (type(c_target) != list) else c_target
            for i in targets:
                srcfile = os.path.join(temp_wd, target_location, i)
                dstfile = BUILD_INSTALLER_FILES
                if "outdir" in component:
                    dstfile = os.path.join(dstfile, component["outdir"])
                dstfile = os.path.join(dstfile, target_name_override if target_name_override else i)
                if i.endswith(".app"):
                    utl.CopyMacBundle(srcfile, dstfile)
                else:
                    utl.CopyFile(srcfile, dstfile)
        if BUILD_SYMBOL_FILES and "symbols" in component:
            srcfile = os.path.join(temp_wd, target_location, component["symbols"])
            if os.path.exists(srcfile):
                dstfile = BUILD_SYMBOL_FILES
                if "outdir" in component:
                    dstfile = os.path.join(dstfile, component["outdir"])
                dstfile = os.path.join(dstfile, component["symbols"])
                utl.CopyFile(srcfile, dstfile)


def build_components(configdata, targetlist, qt_root):
    # Setup globals.
    global BUILD_MAC_DEPLOY
    BUILD_MAC_DEPLOY = os.path.join(qt_root, "bin", "macdeployqt")
    # Create an environment with compile-related vars.
    buildenv = os.environ.copy()
    if CURRENT_OS == "win32":
        buildenv.update({"MAKEFLAGS": "S"})
        buildenv.update(iutl.GetVisualStudioEnvironment(arghelper.target_arm64_arch()))
        buildenv.update({"CL": "/MP"})
    # Build all components needed.
    for target in targetlist:
        if target not in configdata:
            raise iutl.InstallError("Undefined target: {} (please check \"{}\")".format(target, BUILD_CFG_NAME))
        if CURRENT_OS in configdata[target]:
            build_component(configdata[target][CURRENT_OS], qt_root, buildenv)
            deploy_component(configdata, target, buildenv)


def pack_symbols():
    msg.Info("Packing symbols...")
    symbols_archive_name = "WindscribeSymbols_{}.zip".format(extractor.app_version(True))
    zf = zipfile.ZipFile(os.path.join(BUILD_SYMBOL_FILES, "..", symbols_archive_name), "w", zipfile.ZIP_DEFLATED)
    skiplen = len(BUILD_SYMBOL_FILES) + 1
    for filename in glob.glob(BUILD_SYMBOL_FILES + os.sep + "**"):
        if os.path.isdir(filename):
            continue
        filenamepartial = filename[skiplen:]
        msg.Print(filenamepartial)
        zf.write(utl.MakeUnicodePath(filename), filenamepartial)
    zf.close()


def sign_executables_win32(configdata, cert_password, filename_to_sign=None):
    if "signing_cert_win" in configdata and \
            "path_signtool" in configdata["signing_cert_win"] and \
            "path_cert" in configdata["signing_cert_win"] and \
            "timestamp" in configdata["signing_cert_win"] and \
            cert_password:
        signtool = os.path.join(pathhelper.ROOT_DIR, configdata["signing_cert_win"]["path_signtool"])
        certfile = os.path.join(pathhelper.ROOT_DIR, configdata["signing_cert_win"]["path_cert"])
        timestamp = configdata["signing_cert_win"]["timestamp"]

        if filename_to_sign:
            iutl.RunCommand([signtool, "sign", "/fd", "SHA256", "/t", timestamp, "/f", certfile,
                             "/p", cert_password, filename_to_sign])
        else:
            iutl.RunCommand([signtool, "sign", "/fd", "SHA256", "/t", timestamp, "/f", certfile,
                             "/p", cert_password, os.path.join(BUILD_INSTALLER_FILES, "*.exe")])
    else:
        msg.Info("Skip signing. The signing data is not set in YML.")


def build_installer_win32(configdata, qt_root, msvc_root, crt_root, win_cert_password):
    # Copy Windows files.
    msg.Info("Copying libs...")

    if "msvc" in configdata["client_deploy_files"]["win32"]:
        copy_files("MSVC", configdata["client_deploy_files"]["win32"]["msvc"], msvc_root, BUILD_INSTALLER_FILES)

    if crt_root:
        utl.CopyAllFiles(crt_root, BUILD_INSTALLER_FILES)

    if "additional_files" in configdata["client_deploy_files"]["win32"]:
        additional_dir = os.path.join(pathhelper.ROOT_DIR, "installer", "windows", "additional_files")
        copy_files("additional", configdata["client_deploy_files"]["win32"]["additional_files"], additional_dir, BUILD_INSTALLER_FILES)

    copy_libs(configdata, "win32", BUILD_INSTALLER_FILES)
    if arghelper.target_arm64_arch():
        copy_libs(configdata, "win32_arm64", BUILD_INSTALLER_FILES)
        if "additional_files" in configdata["client_deploy_files"]["win32_arm64"]:
            additional_dir = os.path.join(pathhelper.ROOT_DIR, "installer", "windows", "additional_files")
            copy_files("additional arm64", configdata["client_deploy_files"]["win32_arm64"]["additional_files"], additional_dir, BUILD_INSTALLER_FILES)
    else:
        copy_libs(configdata, "win32_x86_64", BUILD_INSTALLER_FILES)
        if "additional_files" in configdata["client_deploy_files"]["win32_x86_64"]:
            additional_dir = os.path.join(pathhelper.ROOT_DIR, "installer", "windows", "additional_files")
            copy_files("additional x86_64", configdata["client_deploy_files"]["win32_x86_64"]["additional_files"], additional_dir, BUILD_INSTALLER_FILES)

    if "license_files" in configdata:
        license_dir = os.path.join(pathhelper.COMMON_DIR, "licenses")
        copy_files("license", configdata["license_files"], license_dir, BUILD_INSTALLER_FILES)

    # Pack symbols for crashdump analysis.
    pack_symbols()

    if arghelper.sign_app():
        # Sign executable files with a certificate.
        msg.Info("Signing executables...")
        sign_executables_win32(configdata, win_cert_password)
        # Sign DLLs we created
        if "win32" in configdata["codesign_files"]:
            msg.Info("Signing DLLs...")
            for binary_name in configdata["codesign_files"]["win32"]["common"]:
                binary_path = os.path.join(BUILD_INSTALLER_FILES, binary_name)
                if os.path.exists(binary_path):
                    sign_executables_win32(configdata, win_cert_password, binary_path)
                else:
                    msg.Warn("Skipping signing of {}.  File not found.".format(binary_path))
            target_arch = "arm64" if arghelper.target_arm64_arch() else "x86_64"
            if target_arch in configdata["codesign_files"]["win32"]:
                for binary_name in configdata["codesign_files"]["win32"][target_arch]:
                    binary_path = os.path.join(BUILD_INSTALLER_FILES, binary_name)
                    if os.path.exists(binary_path):
                        sign_executables_win32(configdata, win_cert_password, binary_path)
                    else:
                        msg.Warn("Skipping signing of {}.  File not found.".format(binary_path))

    # Place everything in a 7z archive.
    installer_info = configdata["installer"]["win32"]
    archive_filename = os.path.normpath(os.path.join(pathhelper.ROOT_DIR, installer_info["subdir"], "resources", "windscribe.7z"))
    # Don't delete the archive after the installer is built so we can use it for installer build/testing/debugging during development.
    if os.path.exists(archive_filename):
        utl.RemoveFile(archive_filename)
    msg.Info("Zipping into " + archive_filename)
    ziptool = os.path.join(pathhelper.TOOLS_DIR, "bin", "7z.exe")
    iutl.RunCommand([ziptool, "a", archive_filename, os.path.join(BUILD_INSTALLER_FILES, "*"),
                     "-y", "-bso0", "-bsp2"])
    # Build and sign the installer.
    buildenv = os.environ.copy()
    buildenv.update({"MAKEFLAGS": "S"})
    buildenv.update(iutl.GetVisualStudioEnvironment(arghelper.target_arm64_arch()))
    buildenv.update({"CL": "/MP"})
    # Force a rebuild of the installer to ensure we pick up a possibly modified Windscribe.7z archive.
    build_component(installer_info, qt_root, buildenv, True)
    deploy_component(configdata, "installer", buildenv)
    final_installer_name = os.path.normpath(os.path.join(BUILD_INSTALLER_FILES, "..",
                                                         "Windscribe_{}.exe".format(extractor.app_version(True))))
    utl.RenameFile(os.path.normpath(os.path.join(BUILD_INSTALLER_FILES,
                                                 installer_info["target"])), final_installer_name)
    if arghelper.sign_app():
        msg.Info("Signing installer...")
        sign_executables_win32(configdata, win_cert_password, final_installer_name)


def build_installer_mac(configdata, qt_root, build_path):
    # Place everything in a 7z archive.
    msg.Info("Zipping...")
    installer_info = configdata["installer"]["macos"]
    arc_path = os.path.join(pathhelper.ROOT_DIR, installer_info["subdir"], "resources", "windscribe.7z")
    archive_filename = os.path.normpath(arc_path)
    if arghelper.clean():
        utl.RemoveFile(archive_filename)
    iutl.RunCommand(["7z", "a", archive_filename,
                     os.path.join(BUILD_INSTALLER_FILES, "Windscribe.app"),
                     "-y", "-bso0", "-bsp2"])
    # Build and sign the installer.
    with utl.PushDir():
        os.chdir(build_path)
        buildenv = os.environ.copy()
    build_component(installer_info, qt_root, buildenv)
    deploy_component(configdata, "installer", target_name_override="WindscribeInstaller.app")
    if arghelper.notarize():
        msg.Print("Notarizing...")
        iutl.RunCommand([pathhelper.notarize_script_filename_absolute(), arghelper.OPTION_CI_MODE])
    # Drop DMG.
    msg.Print("Preparing dmg...")
    dmg_dir = BUILD_INSTALLER_FILES
    if "outdir" in installer_info:
        dmg_dir = os.path.join(dmg_dir, installer_info["outdir"])
    with utl.PushDir(dmg_dir):
        iutl.RunCommand(["python3", "-m", "dmgbuild", "-s",
                         pathhelper.ROOT_DIR + "/installer/mac/dmgbuild/dmgbuild_settings.py",
                         "WindscribeInstaller",
                         "WindscribeInstaller.dmg", "-D", "app=WindscribeInstaller.app", "-D",
                         "background=" + pathhelper.ROOT_DIR + "/installer/mac/dmgbuild/osx_install_background.tiff"])
        final_installer_name = os.path.normpath(os.path.join(dmg_dir, "Windscribe_{}.dmg"
                                                             .format(extractor.app_version(True))))
    utl.RenameFile(os.path.join(dmg_dir, "WindscribeInstaller.dmg"), final_installer_name)
    utl.RemoveFile(arc_path)


def code_sign_linux(binary_name, binary_dir):
    binary = os.path.join(binary_dir, binary_name)
    binary_base_name = os.path.basename(binary)
    # Skip DGA library signing, if it not exists (to avoid error)
    if binary_base_name == "libdga.so" and not os.path.exists(binary):
        pass
    else:
        signatures_dir = os.path.join(os.path.dirname(binary), "signatures")
        if not os.path.exists(signatures_dir):
            msg.Print("Creating signatures path: " + signatures_dir)
            utl.CreateDirectory(signatures_dir, True)
        private_key = pathhelper.COMMON_DIR + "/keys/linux/key.pem"
        signature_file = signatures_dir + "/" + Path(binary).stem + ".sig"
        msg.Info("Signing " + binary + " with " + private_key + " -> " + signature_file)
        cmd = ["openssl", "dgst", "-sign", private_key, "-keyform", "PEM", "-sha256", "-out", signature_file, "-binary", binary]
        iutl.RunCommand(cmd)


def copy_libs(configdata, platform, dst):
    msg.Info("Copying libs ({})...".format(platform))
    if "libs" in configdata["client_deploy_files"][platform]:
        for k, v in configdata["client_deploy_files"][platform]["libs"].items():
            lib_root = iutl.GetDependencyBuildRoot(k)
            if not lib_root:
                if k == "dga":
                    msg.Info("DGA library not found, skipping...")
                else:
                    raise iutl.InstallError("Library \"{}\" is not installed.".format(k))
            else:
                copy_files(k, v, lib_root, dst)


def build_installer_linux(configdata, qt_root):
    # Creates the following:
    # * windscribe_2.x.y_amd64.deb
    # * windscribe_2.x.y_x86_64.rpm
    copy_libs(configdata, "linux", BUILD_INSTALLER_FILES)
    if platform.processor() == "aarch64":
        copy_libs(configdata, "linux_aarch64", BUILD_INSTALLER_FILES)
    elif platform.processor() == "x86_64":
        copy_libs(configdata, "linux_x86_64", BUILD_INSTALLER_FILES)

    msg.Info("Fixing rpaths...")
    if "fix_rpath" in configdata["client_deploy_files"]["linux"]:
        for k in configdata["client_deploy_files"]["linux"]["fix_rpath"]:
            dstfile = os.path.join(BUILD_INSTALLER_FILES, k)
            fix_rpath_linux(dstfile)

    # sign supplementary binaries and move the signatures into InstallerFiles/signatures
    if arghelper.sign_app() and "linux" in configdata["codesign_files"]:
        for binary_name in configdata["codesign_files"]["linux"]:
            code_sign_linux(binary_name, BUILD_INSTALLER_FILES)

    if "license_files" in configdata:
        license_dir = os.path.join(pathhelper.COMMON_DIR, "licenses")
        copy_files("license", configdata["license_files"], license_dir, BUILD_INSTALLER_FILES)

    # create .deb with dest_package
    if arghelper.build_deb():
        msg.Info("Creating .deb package...")

        src_package_path = os.path.join(pathhelper.ROOT_DIR, "installer", "linux", "common")
        deb_files_path = os.path.join(pathhelper.ROOT_DIR, "installer", "linux", "debian_package")
        if platform.processor() == "x86_64":
            dest_package_name = "windscribe_{}_amd64".format(extractor.app_version(True))
        elif platform.processor() == "aarch64":
            dest_package_name = "windscribe_{}_arm64".format(extractor.app_version(True))
        dest_package_path = os.path.join(BUILD_INSTALLER_FILES, "..", dest_package_name)

        utl.CopyAllFiles(src_package_path, dest_package_path)
        utl.CopyAllFiles(deb_files_path, dest_package_path)
        utl.CopyAllFiles(BUILD_INSTALLER_FILES, os.path.join(dest_package_path, "opt", "windscribe"))

        update_version_in_config(os.path.join(dest_package_path, "DEBIAN", "control"))
        update_arch_in_config(os.path.join(dest_package_path, "DEBIAN", "control"))
        iutl.RunCommand(["fakeroot", "dpkg-deb", "--build", dest_package_path])

    if arghelper.build_rpm():
        msg.Info("Creating .rpm package...")

        utl.CopyAllFiles(os.path.join(pathhelper.ROOT_DIR, "installer", "linux", "rpm_package"), os.path.join(pathlib.Path.home(), "rpmbuild"))
        utl.CopyAllFiles(os.path.join(pathhelper.ROOT_DIR, "installer", "linux", "common"), os.path.join(pathlib.Path.home(), "rpmbuild", "SOURCES"))
        utl.CopyAllFiles(BUILD_INSTALLER_FILES, os.path.join(pathlib.Path.home(), "rpmbuild", "SOURCES", "opt", "windscribe"))

        update_version_in_config(os.path.join(pathlib.Path.home(), "rpmbuild", "SPECS", "windscribe_rpm.spec"))
        iutl.RunCommand(["rpmbuild", "-bb", os.path.join(pathlib.Path.home(), "rpmbuild", "SPECS", "windscribe_rpm.spec")])
        utl.CopyFile(os.path.join(pathlib.Path.home(), "rpmbuild", "RPMS", "x86_64", "windscribe-{}-0.x86_64.rpm".format(extractor.app_version(False))),
                     os.path.join(BUILD_INSTALLER_FILES, "..", "windscribe_{}_x86_64.rpm".format(extractor.app_version(True))))


def build_all(win_cert_password):
    # Load config.
    configdata = utl.LoadConfig(os.path.join(pathhelper.TOOLS_DIR, "{}".format(BUILD_CFG_NAME)))
    if not configdata:
        raise iutl.InstallError("Failed to load config \"{}\".".format(BUILD_CFG_NAME))
    if "targets" not in configdata:
        raise iutl.InstallError("No {} targets defined in \"{}\".".format(CURRENT_OS, BUILD_CFG_NAME))

    msg.Info("App version extracted: \"{}\"".format(extractor.app_version(True)))

    # Get Qt directory.
    qt_root = iutl.GetDependencyBuildRoot("qt")
    if not qt_root:
        raise iutl.InstallError("Qt is not installed.")

    # Do some preliminary VS checks on Windows.
    # TODO: JDRM not sure we should be copying the crt_root files.  I removed them all from the install
    #       on the ARM laptop and the app still starts.  Plus, arm64 versions are not available in the
    #       SDK, only 32-bit arm...
    if CURRENT_OS == "win32":
        buildenv = os.environ.copy()
        buildenv.update(iutl.GetVisualStudioEnvironment(arghelper.target_arm64_arch()))
        if arghelper.target_arm64_arch():
            msvc_root = os.path.join(buildenv["VCTOOLSREDISTDIR"], "arm64", "Microsoft.VC142.CRT")
            # crt_root = "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\{}\\ucrt\\DLLS\\arm".format(buildenv["WINDOWSSDKVERSION"])
            crt_root = ""
        else:
            msvc_root = os.path.join(buildenv["VCTOOLSREDISTDIR"], "x64", "Microsoft.VC142.CRT")
            crt_root = "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\{}\\ucrt\\DLLS\\x64".format(buildenv["WINDOWSSDKVERSION"])
        if not os.path.exists(msvc_root):
            raise iutl.InstallError("MSVS installation not found.")
        if crt_root and not os.path.exists(crt_root):
            raise iutl.InstallError("CRT files not found.")

    # Prepare output.
    artifact_dir = os.path.join(pathhelper.ROOT_DIR, "build-exe")
    if arghelper.clean():
        utl.RemoveDirectory(artifact_dir)
    temp_dir = iutl.PrepareTempDirectory("installer")
    build_dir = os.path.join(pathhelper.ROOT_DIR, "build")
    utl.CreateDirectory(build_dir, arghelper.clean())
    global BUILD_INSTALLER_FILES, BUILD_SYMBOL_FILES
    BUILD_INSTALLER_FILES = os.path.join(temp_dir, "InstallerFiles")
    utl.CreateDirectory(BUILD_INSTALLER_FILES, True)
    if CURRENT_OS == "win32":
        BUILD_SYMBOL_FILES = os.path.join(temp_dir, "SymbolFiles")
        utl.CreateDirectory(BUILD_SYMBOL_FILES, True)

    fix_build_libs_rpaths(configdata)

    # Build the components.
    with utl.PushDir(build_dir):
        if arghelper.build_app():
            build_components(configdata, configdata["targets"], qt_root)
        if CURRENT_OS == "win32":
            if arghelper.build_installer():
                build_installer_win32(configdata, qt_root, msvc_root, crt_root, win_cert_password)
        elif CURRENT_OS == "macos":
            if arghelper.build_installer():
                build_installer_mac(configdata, qt_root, build_dir)
        elif CURRENT_OS == "linux":
            build_installer_linux(configdata, qt_root)

    # Copy artifacts.
    msg.Print("Installing artifacts...")
    utl.CreateDirectory(artifact_dir, True)
    if CURRENT_OS == "macos":
        artifact_path = BUILD_INSTALLER_FILES
        installer_info = configdata["installer"]["macos"]
        if "outdir" in installer_info:
            artifact_path = os.path.join(artifact_path, installer_info["outdir"])
    else:
        artifact_path = temp_dir
    for filename in glob.glob(artifact_path + os.sep + "*"):
        if os.path.isdir(filename):
            continue
        filetitle = os.path.basename(filename)
        utl.CopyFile(filename, os.path.join(artifact_dir, filetitle))
        msg.HeadPrint("Ready: \"{}\"".format(filetitle))

    # Cleanup.
    msg.Print("Cleaning temporary directory...")
    utl.RemoveDirectory(temp_dir)


def download_secrets():
    msg.Print("Checking for valid 1password username")
    if not arghelper.valid_one_password_username():
        user = arghelper.OPTION_ONE_PASSWORD_USER
        raise IOError("Specify 1password username with " + user + " <username> or do not sign")
    msg.Print("Logging in to 1password to download files necessary for signing")
    secrethelper.login_and_download_files_from_1password(arghelper)


def pre_checks_and_build_all():
    # USER ERROR - Cannot notarize without signing
    if arghelper.notarize() and not arghelper.sign_app():
        raise IOError("Cannot notarize without signing. Please enable signing or disable notarization")

    # USER INFO - can't notarize on non-Mac
    if CURRENT_OS != utl.CURRENT_OS_MAC and arghelper.notarize():
        msg.Info("Notarization does not exist on non-Mac platforms, notarization will be skipped")

    # USER ERROR: notarization must be run with sudo perms when locally building and cannot be called from build_all
    if arghelper.notarize() and not arghelper.ci_mode():
        raise IOError("Cannot notarize from build_all. Use manual notarization if necessary "
                      "(may break offline notarizing check for user), "
                      "but notarization should be done by the CI for permissions reasons.")

    # TODO: add check for code-sign cert (in keychain) on mac (check during --no-sign as well)
    # get required Apple developer info
    if CURRENT_OS == "macos":
        global MAC_DEV_ID_KEY_NAME
        global MAC_DEV_TEAM_ID
        MAC_DEV_ID_KEY_NAME, MAC_DEV_TEAM_ID = extractor.mac_signing_params()
        msg.Info("using signing identity - " + MAC_DEV_ID_KEY_NAME)

    win_cert_password = ""
    if arghelper.sign_app():
        # download (local build) or access secret files (ci build)
        if not arghelper.ci_mode():
            if not arghelper.use_local_secrets():
                download_secrets()

        # on linux we need keypair to sign -- check that they exist in the correct location
        if CURRENT_OS == utl.CURRENT_OS_LINUX:
            keypath = pathhelper.linux_key_directory()
            pubkey = pathhelper.linux_public_key_filename_absolute()
            privkey = pathhelper.linux_private_key_filename_absolute()
            if not os.path.exists(pubkey) or not os.path.exists(privkey):
                raise IOError("Code signing (--sign) is enabled but key.pub and/or key.pem were not found in '{keypath}'."
                              .format(keypath=keypath))
            generate_include_file_from_pub_key(pathhelper.linux_include_key_filename_absolute(), pubkey)

        # early check for cert password in notarize.yml on windows
        if CURRENT_OS == utl.CURRENT_OS_WIN:
            notarize_yml_path = os.path.join(pathhelper.TOOLS_DIR, "{}".format(pathhelper.NOTARIZE_YML))
            notarize_yml_config = utl.LoadConfig(notarize_yml_path)
            if not notarize_yml_config:
                raise IOError("Cannot sign without notarize.yml on Windows")
            win_cert_password = notarize_yml_config["windows-signing-cert-password"]
            if not win_cert_password:
                raise IOError("Cannot sign with invalid Windows cert password")

        # early check for provision profile on mac
        if CURRENT_OS == utl.CURRENT_OS_MAC:
            if not os.path.exists(pathhelper.mac_provision_profile_filename_absolute()):
                raise IOError("Cannot sign without provisioning profile")

    # should have everything we need to build with the desired settings
    msg.Print("Building {}...".format(BUILD_TITLE))
    build_all(win_cert_password)


def print_banner(title, width):
    msg.Print("*" * width)
    msg.Print(title)
    msg.Print("*" * width)


def print_help_output():
    print_banner("Windscribe build_all help menu", 60)
    msg.Print("Windscribe's build_all script is used to build and sign the desktop app for Windows, Mac and Linux.")
    msg.Print("By default it will attempt to build without signing.")

    # print all the options
    for name, description in arghelper.options:
        msg.Print(name + ": " + description)
    msg.Print("")


# main script logic, secret cleanup, and timing display
if __name__ == "__main__":
    start_time = time.time()
    CURRENT_OS = utl.GetCurrentOS()

    delete_secrets = False
    try:
        if CURRENT_OS not in BUILD_OS_LIST:
            raise IOError("Building {} is not supported on {}.".format(BUILD_TITLE, CURRENT_OS))

        # check for invalid modes
        err = arghelper.invalid_mode()
        if err:
            raise IOError(err)

        # main decision logic
        if arghelper.help():
            print_help_output()
        elif arghelper.download_secrets():
            download_secrets()
        elif arghelper.build_mode():
            # don't delete secrets in local-secret mode or when doing a developer (non-signed) build.
            if not arghelper.use_local_secrets() and arghelper.sign_app():
                delete_secrets = True
            pre_checks_and_build_all()
        else:
            if arghelper.clean_only():
                msg.Print("Cleaning...")
                clean_all_temp_and_build_dirs()
            if arghelper.delete_secrets_only():
                delete_secrets = True
        exitcode = 0
    except iutl.InstallError as e:
        msg.Error(e)
        exitcode = e.exitcode
    except IOError as e:
        msg.Error(e)
        exitcode = 1

    # delete secrets if necessary
    if delete_secrets:
        msg.Print("Deleting secrets...")
        secrethelper.cleanup_secrets()

    # timing output
    if exitcode == 0:
        elapsed_time = time.time() - start_time
        if elapsed_time >= 60:
            msg.HeadPrint("All done: %i minutes %i seconds elapsed" % (elapsed_time / 60, elapsed_time % 60))
        else:
            msg.HeadPrint("All done: %i seconds elapsed" % elapsed_time)
    sys.exit(exitcode)
