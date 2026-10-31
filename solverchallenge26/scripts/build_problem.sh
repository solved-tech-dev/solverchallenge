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

problem="$(printf '%02d' "$((10#${1#problem}))")"
case "${problem}" in
    01|02|03|04|05|06|07|08|09|10) ;;
    *)
        echo "problem must be 01 through 10" >&2
        exit 2
        ;;
esac

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
    01|02|03|05|06|07|08)
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

case "${problem}" in
    01|02|05)
        if [[ -z "${SC26_LINGLONGAMG_LIBRARY:-}" ]]; then
            echo "LinglongAMG is required at runtime for T${problem}" >&2
            exit 2
        fi
        ;;
esac

"${CMAKE_BIN:-cmake}" "${cmake_args[@]}"
"${CMAKE_BIN:-cmake}" --build "${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}" --parallel

echo "build complete: ${SC26_BUILD_ROOT:-${SC26_REPO_ROOT}/build}/problem${problem}"
