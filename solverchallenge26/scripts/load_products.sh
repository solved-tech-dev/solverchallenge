#!/usr/bin/env bash

SC26_REPO_ROOT="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
)"
SC26_LOCAL_ROOT="${SC26_LOCAL_ROOT:-${SC26_REPO_ROOT}/local}"

if [[ -f "${SC26_REPO_ROOT}/configs/local.env" ]]; then
    # shellcheck disable=SC1091
    source "${SC26_REPO_ROOT}/configs/local.env"
fi

if [[ -z "${XIANZHIKRYLOV_ROOT:-}" &&
      -d "${SC26_LOCAL_ROOT}/packages/xianzhikrylov" ]]; then
    export XIANZHIKRYLOV_ROOT="${SC26_LOCAL_ROOT}/packages/xianzhikrylov"
fi
if [[ -z "${PANGULU_ROOT:-}" &&
      -d "${SC26_LOCAL_ROOT}/packages/pangulu" ]]; then
    export PANGULU_ROOT="${SC26_LOCAL_ROOT}/packages/pangulu"
fi
if [[ -z "${SC26_LINGLONGAMG_LIBRARY:-}" &&
      -f "${SC26_LOCAL_ROOT}/packages/linglongamg/lib/liblinglongamg.so" ]]; then
    export SC26_LINGLONGAMG_LIBRARY="${SC26_LOCAL_ROOT}/packages/linglongamg/lib/liblinglongamg.so"
fi
if [[ -z "${SC26_PANGULU_LIBRARY:-}" &&
      -f "${SC26_LOCAL_ROOT}/packages/pangulu/lib/libpangulu.so" ]]; then
    export SC26_PANGULU_LIBRARY="${SC26_LOCAL_ROOT}/packages/pangulu/lib/libpangulu.so"
fi

for candidate in "${SC26_LOCAL_ROOT}"/licenses/xianzhikrylov*.key; do
    if [[ -z "${XIANZHIKRYLOV_LICENSE_PATHS:-}" && -f "${candidate}" ]]; then
        export XIANZHIKRYLOV_LICENSE_PATHS="${candidate}"
        break
    fi
done
for candidate in "${SC26_LOCAL_ROOT}"/licenses/linglongamg*.key; do
    if [[ -z "${LINGLONGAMG_LICENSE_PATHS:-}" && -f "${candidate}" ]]; then
        export LINGLONGAMG_LICENSE_PATHS="${candidate}"
        break
    fi
done
for candidate in "${SC26_LOCAL_ROOT}"/licenses/pangulu*.key; do
    if [[ -z "${PANGULU_LICENSE_PATHS:-}" && -f "${candidate}" ]]; then
        export PANGULU_LICENSE_PATHS="${candidate}"
        break
    fi
done

for library_dir in \
    "${XIANZHIKRYLOV_ROOT:-${SC26_LOCAL_ROOT}/packages/xianzhikrylov}/lib" \
    "${SC26_LOCAL_ROOT}/packages/linglongamg/lib" \
    "${PANGULU_ROOT:-${SC26_LOCAL_ROOT}/packages/pangulu}/lib"; do
    if [[ -d "${library_dir}" &&
          ":${LD_LIBRARY_PATH:-}:" != *":${library_dir}:"* ]]; then
        export LD_LIBRARY_PATH="${library_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
done
