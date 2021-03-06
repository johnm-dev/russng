#! /bin/bash
#
# rurun-legacy.sh

# license--start
#
# Copyright 2012 John Marshall
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

# given a targets specification, generate a list of the individual
# target ids. target spec:
# a - individual item
# a:b - range a to b (non-inclusive), step 1
# a:b:c - range a to b (includes a, not b), step c (where c is
# positive or negative)
function get_target_ids {
	local target_ids=""
	local head rest start end step

	head=""
	rest=$1; shift 1
	while [ -n "${rest}" ]; do
		head=${rest%%,*}
		if [ "${head}" = "${rest}" ]; then
			rest=""
		else
			rest=${rest#*,}
		fi
		start=${head%%:*}
		endstep=${head#*:}
		end=${endstep%%:*}
		step=${endstep#*:}
		if [ "${step}" = "${endstep}" ]; then
			step=""
		fi

		# replace strings with integers
		if [ "${start}" = "" ]; then
			start=0
		fi
		if [ "${end}" = "" ]; then
			# implicit use of RURUN_PNET_ADDR
		        end=$(ruexec "${RURUN_PNET_ADDR}/count")
			if [ $? -ne 0 ]; then
				echo "error: bad end" 1>&2
				exit 1
			fi
		fi
		if [ "${step}" = "" ]; then
			step=1
		fi

		# validate
		if [ "${start}" = "${head}" -a "${end}" = "${head}" ]; then
			# single item
			target_ids="${target_ids} ${start}"
		else
			# range (expect integers)
			if [ ${start} -le ${end} ]; then
				if [ ${step} -le 0 ]; then
					echo "error: bad step (${step})" 1>&2
					exit 1
				fi

				# increment
				while [ ${start} -lt ${end} ]; do
					target_ids="${target_ids} ${start}"
					((start=start+step))
				done
			else
				if [ ${step} -ge 0 ]; then
					echo "error: bad step (${step})" 1>&2
					exit 1
				fi

				#decrement
				while [ ${start} -gt ${end} ]; do
					target_ids="${target_ids} ${start}"
					((start=start+step))
				done
			fi
		fi
	done
	echo ${target_ids}
}

function print_usage {
	echo "\
usage: ${PROG_NAME} [options] <targetspec> <arg> ...
       ${PROG_NAME} [--pnet <addr>] --count

Launch a program on one or more targets. If multiple targets are
specified, the last non-0 exit value will be returned; otherwise an
exit value of 0 is returned.

Targets are specified in a file. See --targets.

If --count is specified, then the number of targets is printed.

targetspec is a comma separated list of one or more of:
* value (e.g., 10)
* start:end (e.g., 0:3 which is equivalent to 0,1,2)
* start:end:step (e.g., 0:4:2 which is equivalent to 0,2); if start
  is greater than end, step must be negative (e.g., 3:0:-1 is
  equivalent to 3,2,1)

Options:
-a|--attr <name>=<value>
	Provide attribute/environment variable settings. A
	comma-separated list of environment variable names in
	\$RURUN_ENV are also passed.
--debug
	Enable debugging. Or set RURUN_DEBUG=1.
--exec simple|shell|login
	Environment to launch with:
	simple - without shell
	shell - shell with basic environment
	login - shell with login environment
	Defaults to \$RURUN_EXEC_METHOD or \"${RURUN_EXEC_METHOD_DEFAULT}\".
-n <maxtasks>
	Set number of concurrently running tasks. Default is ${RURUN_NRUNNING_MAX}.
--pnet <addr>
	Use a given pnet address. Defaults to \$RURUN_PNET_ADDR.
--relay <name>
	Use a given relay service. Defaults to \${RURUN_RELAY} or
	\"${RURUN_RELAY_DEFAULT}\".
--shell <path>
	Alternative shell to run on target. The arguments are passed
	to it for execution. Forces \"--exec simple\".
--targetsfile <path>
	Use targets file. Defaults \${RURUN_TARGETSFILE}.
-t|--timeout <seconds>
	Allow a given amount of time to connect before aborting.
	Defaults to \${RURUN_TIMEOUT} if set.
--wrap	Indexes in the <targetspec> which are outside of the count are
	wrapped back to zero (effectively, modulo <count>). Negative
	indexes are wrapped, too."
}

# defaults (null or unset)
PROG_NAME=$(basename $0)
RURUN_EXEC_METHOD_DEFAULT=shell
RURUN_NRUNNING_MAX=1
RURUN_RELAY_DEFAULT=ssh
RURUN_SHELL=""
export RURUN_EXEC_METHOD=${RURUN_EXEC_METHOD:-${RURUN_EXEC_METHOD_DEFAULT}}
export RURUN_RELAY=${RURUN_RELAY:-${RURUN_RELAY_DEFAULT}}

if [ $# -gt 0 ]; then
	if [ "$1" = "-h" -o "$1" = "--help" ]; then
		print_usage
		exit 0
	fi
elif [ $# -lt 2 ]; then
	echo "error: bad/missing arguments" 1>&2
	exit 1
fi

# test for "wait -n" support
if [ ${BASH_VERSINFO[0]} -gt 4 ]; then
	usewaitn=1
elif [ ${BASH_VERSINFO[0]} -eq 4 -a ${BASH_VERSINFO[1]} -ge 3 ]; then
	usewaitn=1
else
	usewaitn=0
fi

# command-line handling
attrs=()
nrunningmax=${RURUN_NRUNNING_MAX}
mpi=0

while [ $# -gt 0 ]; do
	case $1 in
	-a|--attr)
		shift 1
		attrs+=("-a" "$1"); shift 1
		;;
	--count)
		count_opt=1
		shift 1
		;;
	--debug)
		shift 1
		export RURUN_DEBUG=1
		;;
	--exec)
		shift 1
		RURUN_EXEC_METHOD=$1; shift 1
		;;
	-n)
		shift 1
		nrunningmax=$1; shift
		;;
	--pnet)
		shift 1
		RURUN_PNET_ADDR="$1"; shift 1
		;;
	--relay)
		shift 1
		RURUN_RELAY="$1"; shift 1
		;;
	--shell)
		shift 1
		RURUN_SHELL="$1"; shift 1
		;;
	-t|--timeout)
		shift 1
		RURUN_TIMEOUT="$1"; shift 1
		;;
	--targetsfile)
		shift 1
		RURUN_TARGETSFILE="$1"; shift 1
		;;
	--wrap)
		shift 1
		colon=":"
		;;
	*)
		targetspec=$1; shift 1
		break
		;;
	esac
done

if [ "${usewaitn}" = "0" ]; then
	# old bash, clamping maxtasks to 1
	nrunningmax=1
fi

# expand
RURUN_TARGETSFILE="${RURUN_TARGETSFILE:-${RURUN_PNET_TARGETSFILE}}"
RURUN_TARGETSFILE=$(readlink -f "${RURUN_TARGETSFILE}")

# dynamically start pnet server if necessary
if [ -z "${RURUN_PNET_ADDR}" ]; then
	if [ -n "${RURUN_TARGETSFILE}" ]; then
		x=$(ruspawn --withpids -c main:path=/usr/lib/russng/russpnet/russpnet_server -c "targets:filename=${RURUN_TARGETSFILE}" -c "main:pgid=0")
		export RURUN_PNET_ADDR="${x##*:}"
		x="${x%:*}"
		pnetpid="${x%:*}"
		pnetpgid="${x#*:}"
	fi
	if [ -z "${RURUN_PNET_ADDR}" ]; then
		echo "error: neither RURUN_PNET_ADDR nor RURUN_TARGETSFILE are set" 1>&2
		exit 1
	fi
	# ensure cleanup of pnet server
	trap "kill -HUP ${pnetpid}" EXIT
fi

# convert targetspec to list of targets
targets=$(get_target_ids "${targetspec}")

# debugging output
if [ -n "${RURUN_DEBUG}" ]; then
	echo "-----"
	echo "targetspec (${targetspec})"
	echo "targets (${targets})"
	echo "RUMPIRUN_ENV (${RUMPIRUN_ENV})"
	echo "RURUN_ENV (${RURUN_ENV})"
	echo "RURUN_EXEC_METHOD (${RURUN_EXEC_METHOD})"
	echo "RURUN_PNET_ADDR (${RURUN_PNET_ADDR})"
	echo "RURUN_RELAY (${RURUN_RELAY})"
	echo "RURUN_SHELL (${RURUN_SHELL})"
	echo "RURUN_TARGETSFILE (${RURUN_TARGETSFILE})"
	echo "-----"
fi

if [ -n "${count_opt}" ]; then
	count=$(ruexec "${RURUN_PNET_ADDR}/count")
	ev=$?
	if [ ${ev} -eq 0 ]; then
		echo ${count}
	fi
	exit ${ev}
fi

# get env attributes
env=()
if [ -n "${RURUN_ENV}" ]; then
	for name in $(echo ${RURUN_ENV} | tr ',' ' '); do
		env+=("-a" "${name}=$(printenv ${name})")
	done
fi

# select exec method and "shell"
if [ -n "${RURUN_SHELL}" ]; then
	# override
	export RURUN_EXEC_METHOD="simple"
fi

# set timeout clause
if [ -n "${RURUN_TIMEOUT}" ]; then
	timeoutcl="-t ${RURUN_TIMEOUT}"
fi

# mpirun-specific
# TODO: eliminate
if [ "${HYDRA_LAUNCHER}" = "ssh" ]; then
	# drop "-x"
	shift 1
fi

# ensure self and all children are killed/cleaned up
trap '' HUP
trap 'exit 1' INT TERM

nrunning=0
finalev=0
for target in ${targets}; do
	# set path
	path="${RURUN_PNET_ADDR}/run/${colon}${target}/${RURUN_EXEC_METHOD}"

	# execute
	if [ -n "${RURUN_DEBUG}" ]; then
		echo "[${target}] exec ruexec \"${timeoutcl}\" \"${env[@]}\" \"${attrs[@]}\" \"${path}\" ${RURUN_SHELL} \"$*\""
	fi
	if [ -z "${RURUN_SHELL}" ]; then
		ruexec ${timeoutcl} "${env[@]}" "${attrs[@]}" "${path}" "$*" & cpid=$!
	else
		ruexec ${timeoutcl} "${env[@]}" "${attrs[@]}" "${path}" "${RURUN_SHELL}" "$*" & cpid=$!
	fi
	nrunning=$((nrunning+1))

	# reap child processes if nrunning maxxed out
	if [ ${nrunning} -eq ${nrunningmax} ]; then
		if [ "${usewaitn}" = "1" ]; then
			wait -n
		else
			wait ${cpid}
		fi
		ev=$?
		if [ ${ev} -ne 0 ]; then
			finalev=${ev}
		fi
		nrunning=$((nrunning-1))
	fi
done

# reap remaining child processes
while [ ${nrunning} -gt 0 ]; do
	if [ "${usewaitn}" = "1" ]; then
		wait -n
	else
		wait ${cpid}
	fi
	ev=$?
	if [ ${ev} -ne 0 ]; then
		finalev=${ev}
	fi
	nrunning=$((nrunning-1))
done

exit ${finalev}
