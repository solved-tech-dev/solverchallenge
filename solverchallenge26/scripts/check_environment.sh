#!/usr/bin/env bash
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=load_products.sh
source "${script_dir}/load_products.sh"

data_root="${1:-${SC26_DATA_ROOT:-}}"
if [[ -z "$data_root" &&
      -d "/public/share/ac3k88uz8y/SolverChallenge26-TenProblems" ]]; then
    data_root="/public/share/ac3k88uz8y/SolverChallenge26-TenProblems"
fi
errors=0
warnings=0

print_ok()
{
    printf '[OK]   %s\n' "$1"
}

print_warn()
{
    printf '[WARN] %s\n' "$1"
    warnings=$((warnings + 1))
}

print_error()
{
    printf '[FAIL] %s\n' "$1" >&2
    errors=$((errors + 1))
}

check_command()
{
    local command_name="$1"
    local description="$2"

    if command -v "$command_name" >/dev/null 2>&1; then
        print_ok "$description: $(command -v "$command_name")"
    else
        print_error "$description not found: $command_name"
    fi
}

check_file()
{
    local path="$1"
    local description="$2"

    if [[ -f "$path" ]]; then
        print_ok "$description: $path"
    else
        print_error "$description not found: $path"
    fi
}

check_library()
{
    local directory="$1"
    local pattern="$2"
    local description="$3"
    local matches=()

    if [[ ! -d "$directory" ]]; then
        print_error "$description directory not found: $directory"
        return
    fi

    mapfile -t matches < <(find "$directory" -maxdepth 1 -name "$pattern" -print)
    if [[ ${#matches[@]} -gt 0 ]]; then
        print_ok "$description: ${matches[0]}"
    else
        print_error "$description not found in: $directory"
    fi
}

problem_selected()
{
    local target="$1"
    local problem

    for problem in "${selected_problems[@]}"; do
        if [[ "$problem" == "$target" ]]; then
            return 0
        fi
    done
    return 1
}

needs_xianzhi()
{
    local problem

    for problem in 01 02 03 05 06 07 08; do
        if problem_selected "$problem"; then
            return 0
        fi
    done
    return 1
}

needs_linglong()
{
    local problem

    for problem in 01 02 04 05; do
        if problem_selected "$problem"; then
            return 0
        fi
    done
    return 1
}

needs_pangulu_root()
{
    problem_selected "09" || problem_selected "10"
}

needs_pangulu_provider()
{
    problem_selected "03" || problem_selected "06"
}

problem_spec="${SC26_PROBLEMS:-01 02 03 04 05 06 07 08 09 10}"
problem_spec="${problem_spec//,/ }"
read -r -a requested_problems <<< "$problem_spec"
selected_problems=()

for item in "${requested_problems[@]}"; do
    if [[ ! "$item" =~ ^(problem)?[0-9]{1,2}$ ]]; then
        print_error "invalid problem in SC26_PROBLEMS: $item"
        continue
    fi
    number="${item#problem}"
    number="$((10#$number))"
    if ((number < 1 || number > 10)); then
        print_error "problem number must be 1 through 10: $item"
        continue
    fi
    selected_problems+=("$(printf '%02d' "$number")")
done

if [[ ${#selected_problems[@]} -eq 0 ]]; then
    print_error "no valid problems selected"
    exit 1
fi

printf 'Checking problems: %s\n\n' "${selected_problems[*]}"

check_command "cmake" "CMake"
check_command "cc" "C compiler"
check_command "c++" "C++ compiler"
check_command "python3" "Python 3"
check_command "realpath" "realpath"

if command -v getconf >/dev/null 2>&1; then
    glibc_version="$(getconf GNU_LIBC_VERSION 2>/dev/null || true)"
    if [[ -n "$glibc_version" ]]; then
        print_ok "System C library: ${glibc_version}"
    fi
fi

cmake_version="$(cmake --version 2>/dev/null | head -n 1 || true)"
if [[ -n "$cmake_version" ]]; then
    print_ok "$cmake_version"
fi

if needs_xianzhi; then
    check_file \
        "${XIANZHIKRYLOV_ROOT:-}/include/XianzhiKrylovAPI.h" \
        "XianzhiKrylov headers"
    check_library \
        "${XIANZHIKRYLOV_ROOT:-}/lib" \
        "libXianzhiKrylov.so*" \
        "XianzhiKrylov library"
    if [[ -n "${XIANZHIKRYLOV_LICENSE_PATHS:-}" &&
          -f "${XIANZHIKRYLOV_LICENSE_PATHS}" ]]; then
        print_ok "XianzhiKrylov license: ${XIANZHIKRYLOV_LICENSE_PATHS}"
    else
        print_error "XianzhiKrylov license not found"
    fi
fi

if needs_linglong; then
    check_file \
        "${SC26_LINGLONGAMG_LIBRARY:-}" \
        "LinglongAMG library"
    if [[ -n "${LINGLONGAMG_LICENSE_PATHS:-}" &&
          -f "${LINGLONGAMG_LICENSE_PATHS}" ]]; then
        print_ok "LinglongAMG license: ${LINGLONGAMG_LICENSE_PATHS}"
    else
        print_error "LinglongAMG license not found"
    fi
fi

if needs_pangulu_root; then
    check_file "${PANGULU_ROOT:-}/include/pangulu.h" "PanguLU headers"
    check_library "${PANGULU_ROOT:-}/lib" "libpangulu.so*" "PanguLU library"
fi

if needs_pangulu_provider; then
    check_file "${SC26_PANGULU_LIBRARY:-}" "PanguLU provider library"
fi

if needs_pangulu_root || needs_pangulu_provider; then
    if [[ -n "${PANGULU_LICENSE_PATHS:-}" &&
          -f "${PANGULU_LICENSE_PATHS}" ]]; then
        print_ok "PanguLU license: ${PANGULU_LICENSE_PATHS}"
    else
        print_error "PanguLU license not found"
    fi
fi

if [[ -n "${SC26_COMPAT_ROOT:-}" ]]; then
    check_file \
        "${SC26_COMPAT_ROOT}/ld-linux-x86-64.so.2" \
        "Compatibility loader"
fi

if needs_linglong; then
    msgpack_found=0
    if [[ -n "${SC26_COMPAT_ROOT:-}" &&
          -e "${SC26_COMPAT_ROOT}/libmsgpackc.so.2" ]]; then
        print_ok "libmsgpackc: ${SC26_COMPAT_ROOT}/libmsgpackc.so.2"
        msgpack_found=1
    elif command -v ldconfig >/dev/null 2>&1 &&
         ldconfig -p 2>/dev/null | grep -q 'libmsgpackc\.so\.2'; then
        print_ok "libmsgpackc.so.2 found by ldconfig"
        msgpack_found=1
    fi
    if ((msgpack_found == 0)); then
        print_error "libmsgpackc.so.2 not found; LinglongAMG requires it"
    fi
fi

if [[ -n "${SC26_HIP_RUNTIME_LIBRARY:-}" ]]; then
    check_file "${SC26_HIP_RUNTIME_LIBRARY}" "HIP runtime library"
fi

if [[ -n "${TMPDIR:-}" ]]; then
    if [[ -d "$TMPDIR" && -w "$TMPDIR" ]]; then
        print_ok "Compiler temporary directory: $TMPDIR"
    else
        print_error "TMPDIR is not a writable directory: $TMPDIR"
    fi
else
    print_warn "TMPDIR is not set; /tmp may have insufficient space"
fi

if command -v rocm-smi >/dev/null 2>&1; then
    print_ok "GPU tool: $(command -v rocm-smi)"
elif command -v hip-smi >/dev/null 2>&1; then
    print_ok "GPU tool: $(command -v hip-smi)"
else
    print_warn "No rocm-smi or hip-smi found; verify the GPU allocation on the cluster"
fi

if [[ -n "$data_root" ]]; then
    if [[ -d "$data_root" ]]; then
        print_ok "Data directory: $data_root"
    else
        print_error "Data directory not found: $data_root"
    fi

    for problem in "${selected_problems[@]}"; do
        case_dir="${data_root}/solverchallenge26_${problem}"
        matrix="${case_dir}/solverchallenge26_${problem}_A.dat"
        check_file "$matrix" "T${problem} matrix"

        case "$problem" in
            01|02|03|04|05|06|07)
                check_file \
                    "${case_dir}/solverchallenge26_${problem}_b.dat" \
                    "T${problem} RHS"
                ;;
            08|09|10)
                for rhs_index in 01 02 03 04 05 06 07 08 09 10; do
                    check_file \
                        "${case_dir}/solverchallenge26_${problem}_b_${rhs_index}.dat" \
                        "T${problem} RHS ${rhs_index}"
                done
                ;;
        esac
    done
else
    print_warn "Matrix directory not checked; pass it as the first argument"
fi

printf '\n%d error(s), %d warning(s)\n' "$errors" "$warnings"
if ((errors > 0)); then
    exit 1
fi
print_ok "Environment check passed"
