#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNNERS = {'AgeScreen', 'AgeMarker', 'MicroProtocol', 'MicroDiagnostic', 'RomScreen', 'RomStopScreen', 'RomRegisters'}


def load_cases(paths):
    cases = []
    seen = set()
    models = set(re.findall(r'^    (\w+)(?:,|\[\[)', (ROOT / 'includes/Model.h').read_text(), re.M)) - {'Auto'}
    for path in paths:
        catalog = json.loads(path.read_text())
        if catalog['schemaVersion'] != 1 or not catalog['defaults']['colorCorrection'] or catalog['defaults'][
            'realRTC']:
            raise ValueError('Unsupported ROM catalog settings')
        for case in catalog['cases']:
            budget = case['budget']
            identity = '|'.join(str(case[k]).replace('%', '%25').replace('|', '%7c') for k in
                                ('suite', 'rom', 'model', 'startup', 'variant'))
            if (case['id'] != identity or identity in seen or case['model'] not in models or case[
                'runner'] not in RUNNERS
                    or budget['unit'] != 'RunFrameIntervals' or budget['masterTicks'] != budget['value'] * 140448
                    or not 1 <= budget['value'] <= 1000000):
                raise ValueError(f'Invalid ROM contract: {case["id"]}')
            seen.add(identity)
            cases.append(case)
    return cases


def generate(cases, output):
    output.mkdir(parents=True, exist_ok=True)
    rows, diagnostics = [], []
    for case in cases:
        fields = [json.dumps(case[k]) for k in ('id', 'name', 'suite', 'rom', 'bios', 'expected', 'startup', 'variant')]
        fields += [json.dumps(','.join(case['tags'])), 'Model::' + case['model'], 'CatalogRunner::' + case['runner'],
                   str(case['budget']['value']), str(case['grouped']).lower()]
        rows.append('{' + ', '.join(fields) + '},')
        if case['runner'] == 'MicroDiagnostic':
            diagnostic = case['diagnostic']
            fields = [json.dumps(Path(case['rom']).stem), 'MicrotestOutput::' + diagnostic['output']]
            fields.extend(str(diagnostic.get(k, 0)) for k in ('address', 'expected', 'pcBegin', 'pcEnd'))
            diagnostics.append('{' + ', '.join(fields) + '},')
    (output / 'CatalogCases.inc').write_text('\n'.join(rows) + '\n')
    (output / 'MicrotestCases.inc').write_text('\n'.join(diagnostics) + '\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    parser.add_argument('catalogs', type=Path, nargs='+')
    args = parser.parse_args()
    generate(load_cases(args.catalogs), args.output)


if __name__ == '__main__':
    main()
