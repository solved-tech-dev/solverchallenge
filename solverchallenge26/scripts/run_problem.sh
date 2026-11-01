#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 3 ]]; then
    echo "usage: $0 PROBLEM MATRIX RHS [RHS ...]" >&2
    exit 2
fi
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/load_products.sh"
problem="$(printf '%02d' "$((10#${1#problem}))")"
shift
matrix="$(realpath -e -- "$1")"; shift
rhs_files=()
for rhs in "$@"; do rhs_files+=("$(realpath -e -- "$rhs")"); done
case "${SC26_CASE:-q1}" in q1|q2) ;; *) echo "SC26_CASE must be q1 or q2" >&2; exit 2;; esac
export SC26_CASE="${SC26_CASE:-q1}"
export SC26_CONFIG_DIR="${SC26_CONFIG_DIR:-${SC26_REPO_ROOT}/problems/problem${problem}/config}"
profile="${SC26_CONFIG_DIR}/runtime.env"
[[ $problem != 02 ]] || profile="${SC26_CONFIG_DIR}/runtime_${SC26_CASE}.env"
[[ ! -f $profile ]] || source "$profile"
export OMP_NUM_THREADS="${OMP_NUM_THREADS:-16}" OMP_DYNAMIC=FALSE OPENBLAS_NUM_THREADS=1
export SC26_RUNTIME=hip
if [[ $problem == 04 && ${SC26_T4_DEVICE_INPUT:-0} == 1 ]]; then
    # Requires the LinglongAMG device-input candidate described in problem04/README.md.
    export SA6_DEVICE_INPUT=1 SC26_HOST_MATRIX=0
fi
export SC26_PANGULU_REORDERING=L1 SC26_PANGULU_REUSE=L2
build_dir="${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}"
case "$problem" in
 01|02|03|04|05|07) target=iterative_solver; count=1; args=("$matrix" "${rhs_files[0]}" 0 0);;
 06) target=iterative_solver; count=1; args=("$matrix" "${rhs_files[0]}" 0 1);;
 08) target=iterative_solver_multi; count=10; export SC26_RESIDUAL_MODE=absolute_max; args=("$matrix" 1 1e-8 "${rhs_files[@]}");;
 09|10) target=direct_solver_multi; count=10; export SC26_RESIDUAL_MODE=relative_l2
        sys=0; [[ $problem != 10 ]] || sys=1; args=("$matrix" "$sys" 1e-8 "${rhs_files[@]}");;
 *) echo "problem must be 01 through 10" >&2; exit 2;;
esac
[[ ${#rhs_files[@]} == "$count" ]] || { echo "T${problem} expects $count RHS file(s)" >&2; exit 2; }
executable="${build_dir}/bin/${target}"
[[ -x $executable ]] || { echo "Build first: ./scripts/build_problem.sh $problem" >&2; exit 2; }
executable="$(realpath -e -- "$executable")"
output="${SC26_OUTPUT_DIR:-${SC26_REPO_ROOT}/results/T${problem}_${SC26_CASE}_$(date +%Y%m%dT%H%M%S)}"
mkdir -p "$output"; output="$(realpath -- "$output")"
echo "Output: $output"
cd "$output"
"$executable" "${args[@]}" 2>&1 | tee run.log
