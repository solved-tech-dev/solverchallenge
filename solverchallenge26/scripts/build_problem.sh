#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 PROBLEM" >&2
    echo "example: $0 07" >&2
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
case "${problem}" in
    01|02|03|04|05|06|07|08|09|10)
        ;;
    *)
        echo "problem must be 01 through 10" >&2
        exit 2
        ;;
esac

# Configure the common serial build and select the requested problem slot.
cmake_args=(
    -S "${SC26_REPO_ROOT}"
    -B "${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}"
    -DCMAKE_BUILD_TYPE=Release
    -DSC26_BUILD_MPI=OFF
    -DSC26_PROBLEM="${problem}"
)

case "${problem}" in
    01|02|03)
        cmake_args+=(-DSC26_SOLUTION_FP128=ON)
        ;;
esac

case "${problem}" in
    01|02|03|04|05|06|07|08)
        if [[ -z "${XIANZHIKRYLOV_ROOT:-}" ]]; then
            echo "XianzhiKrylov was not found under local/packages/xianzhikrylov" >&2
            exit 2
        fi
        cmake_args+=(-DSC26_XIANZHIKRYLOV_ROOT="${XIANZHIKRYLOV_ROOT}")
        ;;
esac

case "${problem}" in
    03|06)
        if [[ -z "${SC26_PANGULU_LIBRARY:-}" ]]; then
            echo "PanguLU provider library is required for T${problem}" >&2
            exit 2
        fi
        ;;
    09|10)
        if [[ -z "${PANGULU_ROOT:-}" ]]; then
            echo "PanguLU was not found under local/packages/pangulu" >&2
            exit 2
        fi
        cmake_args+=(-DSC26_PANGULU_ROOT="${PANGULU_ROOT}")
        ;;
esac

# T01, T02, T04, and T05 load LinglongAMG through XianzhiKrylov.
case "${problem}" in
    01|02|04|05)
        if [[ -z "${SC26_LINGLONGAMG_LIBRARY:-}" ]]; then
            echo "LinglongAMG is required at runtime for T${problem}" >&2
            exit 2
        fi
        ;;
esac

"${CMAKE_BIN:-cmake}" "${cmake_args[@]}"

target=iterative_solver
case "${problem}" in
    08)
        target=iterative_solver_multi
        ;;
    09|10)
        target=direct_solver_multi
        ;;
esac

"${CMAKE_BIN:-cmake}" \
    --build "${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}" \
    --target "$target" \
    --parallel

echo "build complete: ${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}"
