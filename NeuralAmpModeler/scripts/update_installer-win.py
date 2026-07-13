#!/usr/bin/env python3

# this script will update the version and text in the innosetup installer files, based on config.h and demo 1/0

import plistlib, os, datetime, fileinput, glob, sys, string

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "../../iPlug2"

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + "/Scripts"))

from parse_config import parse_config


def replacestrs(filename, s, r):
    files = glob.glob(filename)

    for line in fileinput.input(files, inplace=1):
        string.find(line, s)
        line = line.replace(s, r)
        sys.stdout.write(line)


def env_or_default(name, default):
    return os.environ.get(name, default)


def config_string_define(name):
    prefix = "#define " + name + " "
    with open(projectpath + "/config.h", encoding="utf-8") as config_file:
        for line in config_file:
            if line.startswith(prefix):
                return line[len(prefix):].strip().strip('"')
    raise RuntimeError("Missing config definition: " + name)


def main():
    demo = 0

    if len(sys.argv) != 2:
        print("Usage: update_installer_version.py demo(0 or 1)")
        sys.exit(1)
    else:
        demo = int(sys.argv[1])

    config = parse_config(projectpath)
    bundle_name = config["BUNDLE_NAME"]
    display_name = env_or_default("INSTALLER_DISPLAY_NAME", config["PLUG_NAME"])
    publisher = env_or_default("INSTALLER_APP_PUBLISHER", config["PLUG_MFR"])
    semantic_version = config_string_define("PLUG_VERSION_STR")
    display_version = semantic_version.replace("-rc", " RC")
    installer_suffix = " Demo" if demo else ""
    default_output_name = (
        bundle_name + "-Windows-x64-Installer-v" + semantic_version
    )

    setup_values = {
        "AppName": display_name,
        "AppVerName": display_name + " " + display_version,
        "AppContact": env_or_default(
            "INSTALLER_APP_CONTACT",
            "https://github.com/DangerGuitarAmps/DangerGuitarAmps/issues",
        ),
        "AppCopyright": env_or_default(
            "INSTALLER_APP_COPYRIGHT",
            "Copyright (C) 2026 Danger Audio",
        ),
        "AppPublisher": publisher,
        "AppPublisherURL": env_or_default(
            "INSTALLER_APP_PUBLISHER_URL",
            "https://github.com/DangerGuitarAmps/DangerGuitarAmps",
        ),
        "AppSupportURL": env_or_default(
            "INSTALLER_APP_SUPPORT_URL",
            "https://github.com/DangerGuitarAmps/DangerGuitarAmps/issues",
        ),
        "AppVersion": display_version,
        "VersionInfoVersion": config_string_define("PLUG_WINDOWS_VERSION_STR"),
        "VersionInfoCompany": publisher,
        "VersionInfoProductName": display_name,
        "VersionInfoProductVersion": config_string_define(
            "PLUG_WINDOWS_VERSION_STR"
        ),
        "DefaultDirName": "{autopf}\\" + publisher + "\\" + display_name,
        "OutputBaseFilename": env_or_default(
            "INSTALLER_OUTPUT_BASE_FILENAME", default_output_name
        ),
        "WelcomeLabel1": env_or_default(
            "INSTALLER_WELCOME_LABEL",
            "Welcome to the " + display_name + installer_suffix + " installer",
        ),
        "SetupWindowTitle": env_or_default(
            "INSTALLER_SETUP_WINDOW_TITLE",
            display_name + installer_suffix + " installer",
        ),
    }

    # WIN INSTALLER
    print("Updating Windows Installer version info...")

    for line in fileinput.input(
        projectpath + "/installer/" + bundle_name + ".iss", inplace=1
    ):
        if "=" in line:
            key = line.split("=", 1)[0]
            if key in setup_values:
                line = key + "=" + setup_values[key] + "\n"

        if 'Source: "readme' in line:
            if demo:
                line = 'Source: "readme-win-demo.rtf"; DestDir: "{app}"; DestName: "readme.rtf"; Flags: isreadme\n'
            else:
                line = 'Source: "readme-win.rtf"; DestDir: "{app}"; DestName: "readme.rtf"; Flags: isreadme\n'

        sys.stdout.write(line)


if __name__ == "__main__":
    main()
