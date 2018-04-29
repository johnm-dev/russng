#! /usr/bin/env python2
#
# russ_converthosts.py

import os
import os.path
import sys

PROG_NAME = os.path.basename(sys.argv[0])

dry = False

def load_hosts(path):
    hosts = []
    for line in open(path, "r"):
        line = line.strip()
        if line.startswith("#") or line == "":
            continue
        hosts.append(line)
    return hosts

def fmti(s, fmt):
    if fmt == None:
        return s
    l = list(fmt)
    j = 1
    for i in range(1, len(fmt)+1):
        if l[-i] == "0":
            l[-i] = s[-j]
            j = j+1         
            if j > len(s):
                break
    return "".join(l)

def _hosts_to_spaths(hosts, svcfmt, namefmt):
    l = []
    for i, host in enumerate(hosts):
        svcname = svcfmt and (svcfmt % str(i)) or ""
        src = os.path.normpath("+/ssh/%s/%s" % (host, svcname))
        name = fmti(str(i), namefmt)
        l.append((name, src))
    return l

def hosts_to_dir(hosts, basedir, svcfmt, namefmt):
    if os.path.exists(basedir):
        sys.stderr.write("error: basedir (%s) exists\n" % (basedir,))
        sys.exit(1)

    os.makedirs(basedir)
    for name, src in _hosts_to_spaths(hosts, svcfmt, namefmt):
        linkname = os.path.join(basedir, name)
        if namefmt:
            dirname = os.path.dirname(linkname)
            os.makedirs(dirname)
        os.symlink(src, linkname)

def hosts_to_spaths(hosts, svcfmt, namefmt):
    for name, src in _hosts_to_spaths(hosts, svcfmt, namefmt):
        print "%s" % src

def hosts_to_named_spaths(hosts, svcfmt, namefmt):
    for name, src in _hosts_to_spaths(hosts, svcfmt, namefmt):
        print "%s:%s" % (name, src)

def print_usage():
    print("""\
usage: %s dir <hostsfile> <basedir> <svcfmt> [<namefmt>]
       %s spaths <hostsfile> <svcfmt> [<namefmt>]
       %s namedspaths <hostsfile> <svcfmt> [<namefmt>]

Generate spaths using hosts file.\
""" % (PROG_NAME, PROG_NAME, PROG_NAME))

if __name__ == "__main__":
    args = sys.argv[1:]

    namefmt = None
    try:
        typ = args.pop(0)
        if typ in ["-h", "--help"]:
            print_usage()
            sys.exit(0)

        if typ == "dir":
            hosts_path = args.pop(0)
            basedir = args.pop(0)
            svcfmt = args.pop(0)
        elif typ in ["namedspaths", "spaths"]:
            hosts_path = args.pop(0)
            svcfmt = args.pop(0)
        else:
            raise()
        if args:
            namefmt = args.pop(0)
    except SystemExit:
        raise
    except Exception:
        sys.stderr.write("error: bad/missing arguments\n")
        sys.exit(1)

    hosts = load_hosts(hosts_path)

    if typ == "dir":
        hosts_to_dir(hosts, basedir, svcfmt, namefmt)
    elif typ == "namedspaths":
        hosts_to_named_spaths(hosts, svcfmt, namefmt)
    elif typ == "spaths":
        hosts_to_spaths(hosts, svcfmt, namefmt)
