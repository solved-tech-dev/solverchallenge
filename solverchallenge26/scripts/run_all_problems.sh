#!/usr/bin/env bash
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=load_products.sh
source "${script_dir}/load_products.sh"

data_root="${1:-${SC26_DATA_ROOT:-${SC26_LOCAL_ROOT}/data/SolverChallenge26-preparation}}"
timestamp="$(TZ=Asia/Shanghai date +%Y%m%dT%H%M%S%z)"
output_root="${2:-${SC26_REPO_ROOT}/results/run_${timestamp}}"
build_logs="${output_root}/build-logs"
run_logs="${output_root}/run-logs"
summary="${output_root}/summary.tsv"
overall_status=0

if [[ ! -d "${data_root}" ]]; then
    echo "data root not found: ${data_root}" >&2
    echo "usage: $0 DATA_ROOT [OUTPUT_DIR]" >&2
    exit 2
fi

mkdir -p "${build_logs}" "${run_logs}"
printf 'case\tstatus\tlog\n' > "${summary}"

record() {
    local name="$1"
    local status="$2"
    local log="$3"
    printf '%s\t%s\t%s\n' "${name}" "${status}" "${log}" |
        tee -a "${summary}"
}

build_one() {
    local problem="$1"
    local log="${build_logs}/T${problem}.log"
    if "${script_dir}/build_problem.sh" "${problem}" >"${log}" 2>&1; then
        record "T${problem}_build" "PASS" "${log}"
        return 0
    fi
    record "T${problem}_build" "FAIL" "${log}"
    overall_status=1
    return 1
}

run_one() {
    local label="$1"
    local problem="$2"
    local case_name="$3"
    local matrix="$4"
    shift 4
    local rhs_files=("$@")
    local log="${run_logs}/${label}.log"
    if [[ -n "${case_name}" ]]; then
        if SC26_OUTPUT_DIR="${output_root}/solutions/${label}" SC26_CASE="${case_name}" "${script_dir}/run_problem.sh" \
            "${problem}" "${matrix}" "${rhs_files[@]}" >"${log}" 2>&1; then
            record "${label}" "PASS" "${log}"
            return 0
        fi
    else
        if SC26_OUTPUT_DIR="${output_root}/solutions/${label}" "${script_dir}/run_problem.sh" \
            "${problem}" "${matrix}" "${rhs_files[@]}" >"${log}" 2>&1; then
            record "${label}" "PASS" "${log}"
            return 0
        fi
    fi
    record "${label}" "FAIL" "${log}"
    overall_status=1
    return 1
}

for problem in 01 02 03 04 05 06 07 08 09 10; do
    if ! build_one "${problem}"; then
        continue
    fi

    case_dir="${data_root}/solverchallenge26_${problem}"
    matrix="${case_dir}/solverchallenge26_${problem}_A.dat"
    if [[ ! -r "${matrix}" ]]; then
        record "T${problem}" "INPUT_MISSING" "${case_dir}"
        overall_status=1
        continue
    fi

    case "${problem}" in
        01|02)
            rhs="${case_dir}/solverchallenge26_${problem}_b.dat"
            if [[ ! -r "${rhs}" ]]; then
                record "T${problem}" "INPUT_MISSING" "${case_dir}"
                overall_status=1
                continue
            fi
            run_one "T${problem}_q1" "${problem}" q1 "${matrix}" "${rhs}"
            run_one "T${problem}_q2" "${problem}" q2 "${matrix}" "${rhs}"
            ;;
        03|04|05|06|07)
            rhs="${case_dir}/solverchallenge26_${problem}_b.dat"
            if [[ ! -r "${rhs}" ]]; then
                record "T${problem}" "INPUT_MISSING" "${case_dir}"
                overall_status=1
                continue
            fi
            run_one "T${problem}" "${problem}" "" "${matrix}" "${rhs}"
            ;;
        08|09|10)
            rhs_files=(
                "${case_dir}/solverchallenge26_${problem}_b_01.dat"
                "${case_dir}/solverchallenge26_${problem}_b_02.dat"
                "${case_dir}/solverchallenge26_${problem}_b_03.dat"
                "${case_dir}/solverchallenge26_${problem}_b_04.dat"
                "${case_dir}/solverchallenge26_${problem}_b_05.dat"
                "${case_dir}/solverchallenge26_${problem}_b_06.dat"
                "${case_dir}/solverchallenge26_${problem}_b_07.dat"
                "${case_dir}/solverchallenge26_${problem}_b_08.dat"
                "${case_dir}/solverchallenge26_${problem}_b_09.dat"
                "${case_dir}/solverchallenge26_${problem}_b_10.dat"
            )
            missing=0
            for rhs in "${rhs_files[@]}"; do
                [[ -r "${rhs}" ]] || missing=1
            done
            if ((missing)); then
                record "T${problem}" "INPUT_MISSING" "${case_dir}"
                overall_status=1
                continue
            fi
            run_one "T${problem}" "${problem}" "" "${matrix}" "${rhs_files[@]}"
            ;;
    esac
done


printf '\nResults: %s\n' "${output_root}"
printf 'Summary: %s\n' "${summary}"
exit "${overall_status}"
