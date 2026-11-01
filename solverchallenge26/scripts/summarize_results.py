#!/usr/bin/env python3
"""Summarize per-round timing and verification without inferring missing data."""
import csv
import json
from pathlib import Path
import re
import statistics
import sys

NUMBER = r'([0-9.eE+\-]+)'

def summarize(path):
    text = path.read_text(errors='replace')
    result = {'case': path.stem, 'log': str(path.resolve()), 'status': 'incomplete'}
    rounds = []
    matches = list(re.finditer(r'TRIAL trial=(\d+) raw_seconds=' + NUMBER, text))
    for i, match in enumerate(matches):
        tail = text[match.end():matches[i+1].start() if i+1 < len(matches) else len(text)]
        row = {'trial': int(match[1]), 'raw_seconds': float(match[2])}
        for key in ('analysis_seconds', 'numeric_prepare_seconds', 'solve_seconds',
                    'excluded_input_transfer_seconds', 'excluded_format_conversion_seconds',
                    'amg_setup_seconds', 'amg_apply_seconds', 'amg_commit_seconds'):
            found = re.search(r'\b' + key + '=' + NUMBER, tail)
            if found:
                row[key] = float(found[1])
        found = re.search(r'XianzhiKrylov status=\d+ converged=\d+ iterations=(\d+)', tail)
        if found:
            row['iterations'] = int(found[1])
        row['excluded_seconds'] = sum(row.get(k, 0) for k in
            ('excluded_input_transfer_seconds', 'excluded_format_conversion_seconds'))
        row['competition_seconds'] = row['raw_seconds'] - row['excluded_seconds']
        rounds.append(row)
    measured = [row for row in rounds if 5 <= row['trial'] <= 9]
    if len(measured) == 5:
        result['measured_rounds'] = measured
        for key in ('raw_seconds', 'excluded_seconds', 'competition_seconds',
                    'analysis_seconds', 'numeric_prepare_seconds', 'solve_seconds'):
            if all(key in row for row in measured):
                result[key] = statistics.mean(row[key] for row in measured)
        if all('iterations' in row for row in measured):
            result['iterations'] = [row['iterations'] for row in measured]
        if all(all(k in row for k in ('amg_setup_seconds','amg_apply_seconds','amg_commit_seconds'))
               for row in measured):
            result['linglong_seconds'] = statistics.mean(
                row['amg_setup_seconds'] + row['amg_apply_seconds'] + row['amg_commit_seconds']
                - row.get('excluded_input_transfer_seconds', 0) for row in measured)
            result['xianzhi_and_adapter_seconds'] = result['competition_seconds'] - result['linglong_seconds']
    else:
        found = re.search(r'COMPETITION_TIME_SECONDS=' + NUMBER, text)
        if found:
            result['competition_seconds'] = float(found[1])
            result['timing_note'] = 'No per-round transfer breakdown; driver aggregate only.'
    checks = re.findall(r'RESIDUAL rhs=(\d+) relative_l2=' + NUMBER +
                        r' absolute_max=' + NUMBER + r' (PASS|FAIL)', text)
    final_check = re.search(r'VERIFICATION relative_l2=' + NUMBER +
                           r' threshold=' + NUMBER + r' (PASS|FAIL)', text)
    if checks:
        result['rhs_residuals'] = [dict(rhs=int(x[0]), relative_l2=float(x[1]),
                                      absolute_max=float(x[2]), status=x[3]) for x in checks]
        result['status'] = 'PASS' if len(checks) == 10 and all(x[3]=='PASS' for x in checks) else 'FAIL'
        iters = re.findall(r'\brhs=(\d+) iterations=(\d+)', text)
        if iters:
            result['iterations_by_rhs'] = {int(k): int(v) for k, v in iters}
    elif final_check:
        result['relative_l2'] = float(final_check[1])
        result['threshold'] = float(final_check[2])
        result['status'] = final_check[3]
    if 'solver creation failed' in text or 'numeric preparation failed' in text:
        result['status'] = 'FAIL'
    return result

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: python3 scripts/summarize_results.py results/run_<timestamp>')
    root = Path(sys.argv[1])
    rows = [summarize(p) for p in sorted((root/'run-logs').glob('*.log'))]
    (root/'performance.json').write_text(json.dumps(rows, indent=2)+'\n')
    fields = ['case','status','competition_seconds','raw_seconds','excluded_seconds',
              'linglong_seconds','xianzhi_and_adapter_seconds','iterations','relative_l2']
    with (root/'performance.tsv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=fields, delimiter='\t', extrasaction='ignore')
        writer.writeheader(); writer.writerows(rows)
    print(root/'performance.tsv')
