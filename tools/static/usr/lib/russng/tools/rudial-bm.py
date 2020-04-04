#! /usr/bin/python3
#
# rudial-bm.py

# license--start
#
# Copyright 2020 John Marshall
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# license--end

"""Bechmark tool.
"""

from __future__ import print_function

from multiprocessing import Process, Queue
import sys
import time
import traceback

sys.path.insert(1, "/usr/lib/russng")
import pyruss

def worker(q, name, count, timeout, op, spath, args, attrs):
    counts = [0, 0]
    tmin = 10000000
    tmax = -1
    ttotal = 0

    attrs = attrs or None
    for i in range(count):
        t0 = time.time()
        t = pyruss.dialv_wait_inouterr_timeout(timeout, op, spath, args=args, attrs=attrs)
        t1 = time.time()

        tdiff = t1-t0
        ttotal += tdiff
        tmin = min(tmin, tdiff)
        tmax = max(tmax, tdiff)

        if verbose:
            print(t)
        if t[0:2] == (0, 0):
            counts[0] += 1
        else:
            counts[1] += 1

    tavg = ttotal/count

    q.put((name, counts, tmin, tmax, tavg, ttotal))

def print_usage():
    print("""\
usage: rudial-bm [<options>] <nworkers> <count> <op> <spath> [<arg> ...]

Benchmark dialing a service. Output is discarded. Success and failure
exit values are counted.

Options:
-a|-attr <name>=<value>
    Pass a 'name=value' string to the service.
-t|--timeout <ms>
    Time (in ms) allowed to complete the operation before aborting.""")

if __name__ == "__main__":
    debug = False

    try:
        attrs = {}
        count = None
        nworkers = None
        op = None
        spath = None
        timeout = 5000
        verbose = False

        args = sys.argv[1:]
        while args:
            arg = args.pop(0)
            if arg in ["-a", "--attr"] and args:
                attrs = args.pop(0).split("=", 1)
            elif arg == "--debug":
                debug = True
            elif arg in ["-h", "--help"]:
                print_usage()
                sys.exit(0)
            elif arg in ["-t", "--timeout"] and args:
                timeout = int(args.pop(0))
            elif arg == "-v":
                verbose = True
            else:
                nworkers = int(arg)
                count = int(args.pop(0))
                op = args.pop(0)
                spath = args.pop(0)
                break

        if None in [nworkers, count, op, spath]:
            raise Exception()
    except SystemExit:
        raise
    except:
        if debug:
            traceback.print_exc()
        sys.stderr.write("error: bad/missing argument\n")
        sys.exit(1)

    try:
        q = Queue()
        workers = []
        for i in range(nworkers):
            workers.append(Process(target=worker, args=(q, str(i), count, timeout, op, spath, args, attrs)))

        t0 = time.time()
        for i in range(nworkers):
            workers[i].start()

        for i in range(nworkers):
            workers[i].join()
        t1 = time.time()

        gttotal = t1-t0

        counts = [0, 0]
        tmin = 1000000000000000
        tmax = -1
        tavg = 0
        ttotal = 0
        for i in range(nworkers):
            res = q.get()
            counts[0] += res[1][0]
            counts[1] += res[1][1]
            tmin = min(tmin, res[2])
            tmax = max(tmax, res[3])
            tavg += res[4]
            ttotal += res[5]
        tavg = tavg/nworkers

        print("nworkers (%s)" % (nworkers,))
        print("counts (%s, %s)" % (counts[0], counts[1]))
        print("elapsed (%s)" % (gttotal,))
        print("per call min (%s) max (%s) avg (%s)" % (tmin, tmax, tavg))
        print("calls per second (%s)" % ((nworkers*count)/gttotal,))
    except:
        if debug:
            traceback.print_exc()
        sys.stderr.write("error: unexpected problem\n")
        sys.exit(1)
