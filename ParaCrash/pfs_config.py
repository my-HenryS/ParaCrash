#!/usr/bin/env python3

import argparse
import sys
import os
import re
import subprocess
import configparser
from pfs import PFS_Service, PFS
from config import config


def run_config():
    config_file = config.config_file
    config_path = os.path.abspath(config_file)

    pfs_config = configparser.ConfigParser()
    pfs_config.read(config_path)

    # read global section
    global_config = pfs_config['global']
    service_names = global_config['services'].replace(" ", "").split(",")
    mount_point = global_config['mount_point']
    client_name = global_config['client_name']
    pfs_type = global_config['type']
    stripe_size = global_config['stripe_size']

    if "run_sudo" in global_config:
        run_sudo = global_config["run_sudo"]
        if run_sudo == "True":
            assert(os.geteuid() == 0 and "This job should be ran as sudo")

    pfs_services = []

    # read configuration, find pid for each service
    for service_name in service_names:
        service_type = pfs_config[service_name]['type'].strip()
        service_exec = pfs_config[service_name]['exec'].strip()
        service_tag = pfs_config[service_name]['tag'].strip()
        service_host = pfs_config[service_name]['host'].strip()
        service_dir = pfs_config[service_name]['data_path'].strip()
        data_dirs = pfs_config[service_name]['data_dirs'].replace(" ", "").split(",")

        pids = subprocess.check_output("pidof %s" % (service_exec), shell=True).decode().strip().split(" ")

        # read /proc/$pid/cmdline to correlated pid with each service
        for pid in pids:
            cmdline = subprocess.check_output("cat /proc/%s/cmdline" % (pid), shell=True).decode().strip()
            if service_exec in cmdline and service_tag in cmdline and not cmdline[cmdline.find(service_tag)+len(service_tag)].isdigit():
                pfs_services.append(
                    PFS_Service(service_name, service_type, service_exec, service_tag, service_host, service_dir, data_dirs, pid))
                break

        # change the owner of data directories to current user
        # we are going to do some modificaitions here
        #subprocess.call("sudo chown -R $USER %s" % service_dir, shell=True)

    pfs = PFS(pfs_type, pfs_services, mount_point, client_name, stripe_size)

    if "entryinfo_hints" in global_config:
        pfs.set_entryinfo_hints(global_config['entryinfo_hints'])

    return pfs
