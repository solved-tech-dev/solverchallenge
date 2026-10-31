#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
    echo "usage: $0 PROBLEM MATRIX RHS [RHS ...]" >&2
    exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=load_products.sh
source "${script_dir}/load_products.sh"

problem="$(printf '%02d' "$((10#${1#problem}))")"
shift
matrix="$1"
shift
rhs_files=("$@")
build_dir="${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}"

if [[ ! -f "${matrix}" ]]; then
    echo "matrix file not found: ${matrix}" >&2
    exit 2
fi
for rhs in "${rhs_files[@]}"; do
    if [[ ! -f "${rhs}" ]]; then
        echo "RHS file not found: ${rhs}" >&2
        exit 2
    fi
done

case "${problem}" in
    01|02|03|05|07)
        [[ ${#rhs_files[@]} -eq 1 ]] || {
            echo "T${problem} expects one RHS" >&2
            exit 2
        }
        executable="${build_dir}/bin/iterative_solver"
        [[ -x "${executable}" ]] || {
            echo "build T${problem} first: ./scripts/build_problem.sh ${problem}" >&2
            exit 2
        }
        exec "${executable}" "${matrix}" "${rhs_files[0]}" 0 0
        ;;
    06)
        [[ ${#rhs_files[@]} -eq 1 ]] || {
            echo "T06 expects one RHS" >&2
            exit 2
        }
        executable="${build_dir}/bin/iterative_solver"
        [[ -x "${executable}" ]] || {
            echo "build T06 first: ./scripts/build_problem.sh 06" >&2
            exit 2
        }
        exec "${executable}" "${matrix}" "${rhs_files[0]}" 0 1
        ;;
    08)
        [[ ${#rhs_files[@]} -eq 10 ]] || {
            echo "T08 expects ten RHS files" >&2
            exit 2
        }
        export SC26_RESIDUAL_MODE=absolute_max
        executable="${build_dir}/bin/iterative_solver_multi"
        [[ -x "${executable}" ]] || {
            echo "build T08 first: ./scripts/build_problem.sh 08" >&2
            exit 2
        }
        exec "${executable}" "${matrix}" 1 1e-8 "${rhs_files[@]}"
        ;;
    09)
        [[ ${#rhs_files[@]} -eq 10 ]] || {
            echo "T09 expects ten RHS files" >&2
            exit 2
        }
        export SC26_RESIDUAL_MODE=relative_l2
        executable="${build_dir}/bin/direct_solver_multi"
        [[ -x "${executable}" ]] || {
            echo "build T09 first: ./scripts/build_problem.sh 09" >&2
            exit 2
        }
        exec "${executable}" "${matrix}" 0 1e-8 "${rhs_files[@]}"
        ;;
    10)
        [[ ${#rhs_files[@]} -eq 10 ]] || {
            echo "T10 expects ten RHS files" >&2
            exit 2
        }
        export SC26_RESIDUAL_MODE=relative_l2
        executable="${build_dir}/bin/direct_solver_multi"
        [[ -x "${executable}" ]] || {
            echo "build T10 first: ./scripts/build_problem.sh 10" >&2
            exit 2
        }
        exec "${executable}" "${matrix}" 1 1e-8 "${rhs_files[@]}"
        ;;
    04)
        echo "T04 is deferred" >&2
        exit 2
        ;;
    *)
        echo "problem must be 01 through 10" >&2
        exit 2
        ;;
esac
