#! /bin/bash
#
# rumpirun.sh (symlinked to rumpirun.openmpi and rumpirun.mpich)

# license--start
#
# Copyright 2014 John Marshall
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

# optional/required variables:
# RUMPIRUN_ENV (opt)
# RUMPIRUN_HOSTFILE (req)
# RUMPIRUN_LAUNCHER (opt)
# RUMPIRUN_MPIRUN (opt)
# RUMPIRUN_MPIRUN_IMPL (opt)
# RUMPIRUN_SHELL (opt)

function print_usage {
	echo "usage: ${PROG_NAME} [options] ...

RUSS-based mpirun wrapper. If called as rumpirun, automatically
senses and calls mpirun of openmpi or mpich."
}

function is_openmpi {
	local name=$1

	st=$(${name} -V 2>&1)
	[ "${st#*Open MPI}" != "${st}" ]
	return $?
}

function is_mpich {
	local name=$1

	st=$(${name} --info 2>&1)
	return $?
}

#
# main
#

PROG_NAME=$(basename $0)

export RUMPIRUN_MPIRUN=${RUMPIRUN_MPIRUN:-mpirun}
export RUMPIRUN_LAUNCHER=${RUMPIRUN_LAUNCHER:-rurun}
export RUMPIRUN_SHELL=""
export RUMPIRUN_TARGETSFILE="${RUMPIRUN_TARGETSFILE:-${RUMPIRUN_PNET_TARGETSFILE}}"

# get absolute path
RUMPIRUN_MPIRUN=$(which ${RUMPIRUN_MPIRUN} 2> /dev/null)
if [ -z "${RUMPIRUN_MPIRUN}" ]; then
	echo "error: cannot find mpirun" 1>&2
	exit 1
fi

# identify implementation
if [ -n "${RUMPIRUN_MPIRUN_IMPL}" ]; then
	echo "info: using mpirun implementation override (${RUMPIRUN_MPIRUN_IMPL})" 1>&2
elif is_openmpi ${RUMPIRUN_MPIRUN}; then
	export RUMPIRUN_MPIRUN_IMPL="openmpi"
elif is_mpich ${RUMPIRUN_MPIRUN}; then
	export RUMPIRUN_MPIRUN_IMPL="mpich"
else
	echo "error: unknown implementation of mpirun"
	exit 1
fi

echo "info: MPI implementation (${RUMPIRUN_MPIRUN_IMPL})" 1>&2

# setup by implementation
case "${RUMPIRUN_MPIRUN_IMPL}" in
mpich)
	export HYDRA_BOOTSTRAP=rsh
	export HYDRA_BOOTSTRAP_EXEC="${RUMPIRUN_LAUNCHER}"
	export HYDRA_LAUNCHER=rsh
	export HYDRA_LAUNCHER_EXEC="${RUMPIRUN_LAUNCHER}"
	export HYDRA_HOST_FILE="${RUMPIRUN_HOSTFILE}"
	export HYDRA_LAUNCHER_AUTOFORK=0

	MPIRUN_ARGS="-disable-hostname-propagation"
	;;
openmpi)
	export OMPI_MCA_orte_default_hostfile="${RUMPIRUN_HOSTFILE}"
	export OMPI_MCA_plm_rsh_agent="${RUMPIRUN_LAUNCHER}"
	export OMPI_MCA_plm_rsh_disable_qrsh=1
	export OMPI_MCA_plm_rsh_no_tree_spawn=1

	MPIRUN_ARGS=""
	;;
*)
	echo "error: unknown implementation (${RUMPIRUN_MPIRUN_IMPL}) of mpirun" 1>&2
	exit 1
	;;
esac

# dynamically start pnet server if necessary
if [ -z "${RUMPIRUN_PNET_ADDR}" ]; then
	if [ -n "${RUMPIRUN_TARGETSFILE}" ]; then
		export RUMPIRUN_PNET_ADDR=$(ruspawn -c main:path=/usr/lib/russng/russpnet/russpnet_server -c "targets:filename=${RUMPIRUN_TARGETSFILE}" -c "main:pgid=$$")
	fi
	if [ -n "${RUMPIRUN_PNET_ADDR}" ]; then
		# ensure cleanup of pnet server
		trap "kill -HUP -$$" EXIT
	else
		# fallback to RURUN_PNET_ADDR for backward compatibility ... for now
		echo "warning: RUMPIRUN_PNET_ADDR is not set; falling back to RURUN_PNET_ADDR" 1>&2
		export RUMPIRUN_PNET_ADDR="${RURUN_PNET_ADDR}"
	fi
fi

# set RURUN_* based on RUMPIRUN_*
if [ -n "${RUMPIRUN_ENV}" ]; then
	export RURUN_ENV="${RUMPIRUN_ENV}"
fi
export RURUN_SHELL="${RUMPIRUN_SHELL}"
export RURUN_PNET_ADDR="${RUMPIRUN_PNET_ADDR}"

# reset some items
unset PE_HOSTFILE

"${RUMPIRUN_MPIRUN}" ${MPIRUN_ARGS} "${@}"
