#!/usr/bin/env bash
set -euo pipefail

usage()
{
    echo "usage: $0 PROBLEM MATRIX RHS [RHS ...]" >&2
}

if [[ $# -lt 3 ]]; then
    usage
    exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=load_products.sh
source "${script_dir}/load_products.sh"

requested_problem="${1#problem}"
if [[ ! "$requested_problem" =~ ^[0-9]{1,2}$ ]]; then
    echo "problem must be 01 through 10" >&2
    exit 2
fi
problem_number="$((10#$requested_problem))"
if ((problem_number < 1 || problem_number > 10)); then
    echo "problem must be 01 through 10" >&2
    exit 2
fi
problem="$(printf '%02d' "$problem_number")"
shift

matrix_input="$1"
if [[ ! -f "$matrix_input" ]]; then
    echo "matrix file not found: $matrix_input" >&2
    exit 2
fi
matrix="$(realpath -e -- "$matrix_input")"
shift

rhs_files=()
for rhs in "$@"; do
    if [[ ! -f "$rhs" ]]; then
        echo "RHS file not found: $rhs" >&2
        exit 2
    fi
    rhs_files+=("$(realpath -e -- "$rhs")")
done

case "${SC26_CASE:-q1}" in
    q1|q2)
        ;;
    *)
        echo "SC26_CASE must be q1 or q2" >&2
        exit 2
        ;;
esac

export SC26_CASE="${SC26_CASE:-q1}"
config_dir="${SC26_CONFIG_DIR:-${SC26_REPO_ROOT}/problems/problem${problem}/config}"
export SC26_CONFIG_DIR="$config_dir"

# Every problem can supply a runtime profile.  T02 has one profile for each
# accuracy requirement.
profile="${SC26_CONFIG_DIR}/runtime.env"
if [[ "$problem" == "02" ]]; then
    profile="${SC26_CONFIG_DIR}/runtime_${SC26_CASE}.env"
fi
if [[ -f "$profile" ]]; then
    # shellcheck disable=SC1090
    source "$profile"
fi

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-16}"
export OMP_DYNAMIC=FALSE
export OPENBLAS_NUM_THREADS=1
export SC26_RUNTIME=hip
export SC26_PANGULU_REORDERING=L1
export SC26_PANGULU_REUSE=L2

target=""
rhs_count=0
program_args=()

case "$problem" in
    01|02|03|04|05|07)
        target="iterative_solver"
        rhs_count=1
        program_args=("$matrix" "${rhs_files[0]}" 0 0)
        ;;
    06)
        target="iterative_solver"
        rhs_count=1
        program_args=("$matrix" "${rhs_files[0]}" 0 1)
        ;;
    08)
        target="iterative_solver_multi"
        rhs_count=10
        export SC26_RESIDUAL_MODE=absolute_max
        program_args=("$matrix" 1 1e-8 "${rhs_files[@]}")
        ;;
    09|10)
        target="direct_solver_multi"
        rhs_count=10
        export SC26_RESIDUAL_MODE=relative_l2
        system_type=0
        if [[ "$problem" == "10" ]]; then
            system_type=1
        fi
        program_args=("$matrix" "$system_type" 1e-8 "${rhs_files[@]}")
        ;;
    *)
        echo "problem must be 01 through 10" >&2
        exit 2
        ;;
esac

if [[ ${#rhs_files[@]} -ne "$rhs_count" ]]; then
    echo "T${problem} expects ${rhs_count} RHS file(s)" >&2
    exit 2
fi

build_dir="${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}"
executable="${build_dir}/bin/${target}"
if [[ ! -x "$executable" ]]; then
    echo "Build first: ./scripts/build_problem.sh ${problem}" >&2
    exit 2
fi
executable="$(realpath -e -- "$executable")"

timestamp="$(date +%Y%m%dT%H%M%S)"
output="${SC26_OUTPUT_DIR:-${SC26_REPO_ROOT}/results/T${problem}_${SC26_CASE}_${timestamp}}"
mkdir -p "$output"
output="$(realpath -- "$output")"
echo "Output: $output"
cd "$output"

runner=()
if [[ -n "${SC26_COMPAT_ROOT:-}" ]]; then
    compat="$(realpath -e -- "$SC26_COMPAT_ROOT")"
    if [[ ! -x "$compat/ld-linux-x86-64.so.2" ]]; then
        echo "Compatibility loader missing: $compat/ld-linux-x86-64.so.2" >&2
        exit 2
    fi
    runner=(
        "$compat/ld-linux-x86-64.so.2"
        --library-path
        "${LD_LIBRARY_PATH:-}:$compat"
    )
fi

# Keep the complete solver output for diagnostics, and create the compact
# performance table immediately after the run.
solver_status=0
"${runner[@]}" "$executable" "${program_args[@]}" 2>&1 |
    tee "$output/run.log" || solver_status=$?

summary_status=0
SC26_RESULT_CASE="T${problem}_${SC26_CASE}" \
    python3 "${script_dir}/summarize_results.py" "$output/run.log" ||
    summary_status=$?
if [[ -f "$output/performance.tsv" ]]; then
    echo
    cat "$output/performance.tsv"
fi

if ((summary_status != 0)); then
    exit "$summary_status"
fi
exit "$solver_status"
