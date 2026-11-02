#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=load_products.sh
source "${script_dir}/load_products.sh"

default_data_root="${SC26_LOCAL_ROOT}/data/SolverChallenge26-preparation"
data_root="${1:-${SC26_DATA_ROOT:-$default_data_root}}"

timestamp="$(TZ=Asia/Shanghai date +%Y%m%dT%H%M%S%z)"
output_root="${2:-${SC26_REPO_ROOT}/results/run_${timestamp}}"
build_logs="${output_root}/build-logs"
run_logs="${output_root}/run-logs"
summary="${output_root}/summary.tsv"
overall_status=0

problem_spec="${SC26_PROBLEMS:-01 02 03 04 05 06 07 08 09 10}"
problem_spec="${problem_spec//,/ }"
read -r -a requested_problems <<< "$problem_spec"
selected_problems=()

for item in "${requested_problems[@]}"; do
    if [[ ! "$item" =~ ^(problem)?[0-9]{1,2}$ ]]; then
        echo "invalid problem in SC26_PROBLEMS: $item" >&2
        exit 2
    fi

    number="${item#problem}"
    number="$((10#$number))"
    if ((number < 1 || number > 10)); then
        echo "problem number must be 1 through 10: $item" >&2
        exit 2
    fi

    selected_problems+=("$(printf '%02d' "$number")")
done

if [[ ${#selected_problems[@]} -eq 0 ]]; then
    echo "no problems selected by SC26_PROBLEMS" >&2
    exit 2
fi

if [[ ! -d "$data_root" ]]; then
    echo "data root not found: $data_root" >&2
    echo "usage: $0 DATA_ROOT [OUTPUT_DIR]" >&2
    exit 2
fi

mkdir -p "$build_logs" "$run_logs"
printf 'case\tstatus\tlog\n' > "$summary"

record()
{
    local name="$1"
    local status="$2"
    local log_path="$3"

    printf '%s\t%s\t%s\n' "$name" "$status" "$log_path" |
        tee -a "$summary"
}

build_one()
{
    local problem="$1"
    local log_path="${build_logs}/T${problem}.log"

    if "${script_dir}/build_problem.sh" "$problem" >"$log_path" 2>&1; then
        record "T${problem}_build" "PASS" "$log_path"
        return 0
    fi

    record "T${problem}_build" "FAIL" "$log_path"
    overall_status=1
    return 1
}

run_one()
{
    local label="$1"
    local problem="$2"
    local case_name="$3"
    local matrix="$4"
    local log_path="${run_logs}/${label}.log"
    shift 4
    local rhs_files=("$@")
    local environment=(
        "SC26_OUTPUT_DIR=${output_root}/solutions/${label}"
    )

    if [[ -n "$case_name" ]]; then
        environment+=("SC26_CASE=${case_name}")
    fi

    if env "${environment[@]}" "${script_dir}/run_problem.sh" \
        "$problem" "$matrix" "${rhs_files[@]}" >"$log_path" 2>&1; then
        record "$label" "PASS" "$log_path"
        return 0
    fi

    record "$label" "FAIL" "$log_path"
    overall_status=1
    return 1
}

input_missing()
{
    local problem="$1"
    local path="$2"

    record "T${problem}" "INPUT_MISSING" "$path"
    overall_status=1
}

for problem in "${selected_problems[@]}"; do
    if ! build_one "$problem"; then
        continue
    fi

    case_dir="${data_root}/solverchallenge26_${problem}"
    matrix="${case_dir}/solverchallenge26_${problem}_A.dat"
    if [[ ! -r "$matrix" ]]; then
        input_missing "$problem" "$case_dir"
        continue
    fi

    case "$problem" in
        01|02)
            rhs="${case_dir}/solverchallenge26_${problem}_b.dat"
            if [[ ! -r "$rhs" ]]; then
                input_missing "$problem" "$case_dir"
                continue
            fi
            run_one "T${problem}_q1" "$problem" q1 "$matrix" "$rhs"
            run_one "T${problem}_q2" "$problem" q2 "$matrix" "$rhs"
            ;;
        03|04|05|06|07)
            rhs="${case_dir}/solverchallenge26_${problem}_b.dat"
            if [[ ! -r "$rhs" ]]; then
                input_missing "$problem" "$case_dir"
                continue
            fi
            run_one "T${problem}" "$problem" "" "$matrix" "$rhs"
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
                if [[ ! -r "$rhs" ]]; then
                    missing=1
                fi
            done
            if ((missing)); then
                input_missing "$problem" "$case_dir"
                continue
            fi
            run_one "T${problem}" "$problem" "" "$matrix" "${rhs_files[@]}"
            ;;
    esac
done

performance_status=0
python3 "${script_dir}/summarize_results.py" "$output_root" ||
    performance_status=$?

printf '\nResults: %s\n' "$output_root"
printf 'Summary: %s\n' "$summary"
printf 'Performance: %s\n' "${output_root}/performance.tsv"
if [[ -f "${output_root}/performance.tsv" ]]; then
    echo
    cat "${output_root}/performance.tsv"
fi

if ((performance_status != 0)); then
    overall_status=1
fi
exit "$overall_status"
