#!/usr/bin/env bash

SC26_REPO_ROOT="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
)"
SC26_LOCAL_ROOT="${SC26_LOCAL_ROOT:-${SC26_REPO_ROOT}/local}"

# User-provided paths take priority.  The example file is optional.
if [[ -f "${SC26_REPO_ROOT}/configs/local.env" ]]; then
    # shellcheck disable=SC1091
    source "${SC26_REPO_ROOT}/configs/local.env"
fi

# Keep compiler temporary files on writable storage.  Some cluster accounts
# cannot create /work2/${USER}, and the system /tmp may be too small for CMake
# feature tests and HIP compilation.
sc26_select_tmpdir()
{
    local configured_tmpdir="${TMPDIR:-}"
    local candidate
    local candidates=()

    if [[ -n "$configured_tmpdir" &&
          -d "$configured_tmpdir" &&
          -w "$configured_tmpdir" ]]; then
        return 0
    fi

    if [[ -n "${USER:-}" ]]; then
        candidates+=("/work2/${USER}/tmp/solverchallenge26")
    fi
    candidates+=(
        "${SC26_REPO_ROOT}/local/tmp/solverchallenge26"
    )
    if [[ -n "${HOME:-}" ]]; then
        candidates+=("${HOME}/sc26-work/tmp/solverchallenge26")
    fi

    for candidate in "${candidates[@]}"; do
        if mkdir -p "$candidate" 2>/dev/null &&
           [[ -d "$candidate" && -w "$candidate" ]]; then
            if [[ -n "$configured_tmpdir" ]]; then
                echo "TMPDIR is not writable; using $candidate" >&2
            fi
            export TMPDIR="$candidate"
            return 0
        fi
    done

    if [[ -n "$configured_tmpdir" ]]; then
        echo "TMPDIR is not writable: $configured_tmpdir" >&2
    else
        echo "No writable compiler temporary directory was found" >&2
    fi
}

sc26_select_tmpdir

# Locate product packages extracted under the standard local/ layout.
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

# Use the first matching license unless the caller already set a path.
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

# Current product packages discover licenses inside their own SDK directory.
# Keep local/licenses/ as the user's download location and install private
# copies there before starting a product program.
sc26_install_license()
{
    local license_path="$1"
    local product_root="$2"
    local license_dir
    local destination
    local temporary

    [[ -n "$license_path" && -f "$license_path" && -d "$product_root" ]] || return 0
    license_dir="$product_root/licenses"
    destination="$license_dir/$(basename "$license_path")"
    if [[ -f "$destination" ]] && cmp -s "$license_path" "$destination"; then
        return 0
    fi
    if ! mkdir -p "$license_dir"; then
        echo "Cannot install license in $product_root; use a writable copy of the product package." >&2
        return 2
    fi
    temporary="$(mktemp "$license_dir/.install-XXXXXX")" || return 2
    if ! cp "$license_path" "$temporary" || ! chmod 600 "$temporary" ||
       ! mv -f "$temporary" "$destination"; then
        rm -f "$temporary"
        echo "Cannot install license: $destination" >&2
        return 2
    fi
}

sc26_install_license \
    "${XIANZHIKRYLOV_LICENSE_PATHS:-}" \
    "${XIANZHIKRYLOV_ROOT:-${SC26_LOCAL_ROOT}/packages/xianzhikrylov}" || return $?
sc26_install_license \
    "${LINGLONGAMG_LICENSE_PATHS:-}" \
    "${SC26_LOCAL_ROOT}/packages/linglongamg" || return $?
sc26_install_license \
    "${PANGULU_LICENSE_PATHS:-}" \
    "${PANGULU_ROOT:-${SC26_LOCAL_ROOT}/packages/pangulu}" || return $?

# Make product libraries visible to the generated test programs.
for library_dir in \
    "${XIANZHIKRYLOV_ROOT:-${SC26_LOCAL_ROOT}/packages/xianzhikrylov}/lib" \
    "${SC26_LOCAL_ROOT}/packages/linglongamg/lib" \
    "${PANGULU_ROOT:-${SC26_LOCAL_ROOT}/packages/pangulu}/lib"; do
    if [[ -d "${library_dir}" &&
          ":${LD_LIBRARY_PATH:-}:" != *":${library_dir}:"* ]]; then
        export LD_LIBRARY_PATH="${library_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
done
