#!/usr/bin/env python3
import argparse
import concurrent.futures
import itertools
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BINARY = ROOT / 'build/release/Release/StarGBC_Experiment'
FIELDS = {'mode': 'mode', 'family': 'family', 'model': 'model', 'seed': 'seed', 'phase': 'phase', 'scx': 'scx',
          'wx': 'wx',
          'spriteX': 'sprite-x', 'spriteOffset': 'sprite-offset', 'budgetTicks': 'ticks',
          'expectedValue': 'expected-value',
          'basis': 'basis', 'basisEvidence': 'evidence'}
BOOLEAN_FIELDS = {'doubleSpeed': 'double-speed', 'coordinatePriority': 'coordinate-priority'}
BOUNDARIES = {'Window': {'WindowActivate', 'PpuMode', 'PixelDiscard'}, 'Sprites': {'PixelStall', 'PpuMode'},
              'Stat': {'InterruptRequest', 'InterruptAccept', 'InterruptVector'},
              'TimerReload': {'TimerOverflow', 'TimerReload'},
              'Dma': {'DmaOwnership'}}


def execute(scenario, binary, output):
    if scenario.get('schemaVersion', 1) != 1 or set(scenario) - (
            FIELDS.keys() | BOOLEAN_FIELDS.keys() | {'schemaVersion', 'noise'}):
        raise ValueError('Unsupported timing scenario fields or version')
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [str(binary.resolve()), '--output=' + str(output.resolve())]
    command.extend(
        '--' + flag + '=' + str(scenario[key]) for key, flag in FIELDS.items() if scenario.get(key) is not None)
    command.extend('--' + flag for key, flag in BOOLEAN_FIELDS.items() if scenario.get(key, False))
    command.extend('--noise=' + str(value) for value in scenario.get('noise', []))
    process = subprocess.run(command, capture_output=True, text=True)
    result_path = output / 'result.json'
    if not result_path.is_file():
        raise ValueError('Experiment failed before producing a result: ' + process.stderr)
    result = json.loads(result_path.read_text())
    if result.get('record') != 'experiment' or result.get('schemaVersion') != 1:
        raise ValueError('Invalid experiment result')
    (output / 'command.json').write_text(
        json.dumps(dict(command=command, returncode=process.returncode, stderr=process.stderr), indent=2) + '\n')
    return result


def boundary_observations(result, directory):
    rows = [json.loads(line) for line in (directory / 'events.jsonl').read_text().splitlines()]
    if rows[0]['overwritten']:
        raise ValueError('Experiment trace overflow')
    events = [event for event in rows[1:] if event['kind'] in BOUNDARIES[result['scenario']['family']]]
    return [dict(action=action, absoluteTick=result['anchorTick'] + action['offsetTicks'],
                 boundaries=[dict(kind=event['kind'], tick=event['position']['tick'], value=event['value'],
                                  actionMinusBoundaryTicks=result['anchorTick'] + action['offsetTicks'] -
                                                           event['position']['tick'])
                             for event in events]) for action in result['actions']]


def equivalent(left, right):
    return all(left[key] == right[key] for key in
               ('scenario', 'completionContract', 'outcome', 'failureSignature', 'completedTick',
                'anchorTick', 'actions', 'checks', 'probeValue', 'framebufferHash', 'eventHash'))


def preserves_failure(original, candidate):
    return (original['outcome'] == candidate['outcome'] == 'AssertionFailure'
            and bool(original['failureSignature']) and original['failureSignature'] == candidate['failureSignature']
            and original['completionContract'] == candidate['completionContract']
            and original['scenario']['budgetTicks'] == candidate['scenario']['budgetTicks']
            and all(original['scenario'][key] == candidate['scenario'][key]
                    for key in ('mode', 'family', 'model', 'doubleSpeed', 'expectedValue', 'basis', 'basisEvidence'))
            and all(check['passed'] for check in candidate['checks'] if check['invariant'] in
                    ('cpu-anchor', 'cpu-action-timing', 'cpu-action-value', 'component-action-timing', 'rendered-line',
                     'dma-transfers', 'dma-transfer-values')))


def reduce_scenario(scenario, evaluate, max_trials=100):
    original = evaluate(scenario)
    if original['outcome'] != 'AssertionFailure' or not original['failureSignature']:
        raise ValueError('Reduction requires a completed assertion failure, never a timeout')
    current = original['scenario'].copy()
    trials = []

    def attempt(candidate):
        nonlocal current
        if len(trials) >= max_trials or candidate == current:
            return False
        result = evaluate(candidate)
        accepted = preserves_failure(original, result)
        trials.append(dict(scenario=candidate, outcome=result['outcome'], signature=result['failureSignature'],
                           accepted=accepted))
        if accepted:
            current = result['scenario'].copy()
        return accepted

    chunk = max(1, len(current.get('noise', [])) // 2)
    while current.get('noise') and len(trials) < max_trials:
        changed = False
        for begin in range(0, len(current['noise']), chunk):
            candidate = dict(current, noise=current['noise'][:begin] + current['noise'][begin + chunk:])
            if attempt(candidate):
                changed = True
                break
        if not changed:
            if chunk == 1:
                break
            chunk = max(1, chunk // 2)
    for field in ('phase', 'scx', 'wx', 'spriteOffset', 'seed'):
        if current.get(field):
            attempt(dict(current, **{field: 0}))
    return dict(schemaVersion=1, original=original, reducedScenario=current, trials=trials,
                reducedFailure=evaluate(current),
                minimality='Bounded greedy reduction over setup writes and numeric parameters')


def promote(scenario, result, evidence, output):
    if result['outcome'] != 'Pass' or scenario['basis'] == 'Hypothesis' or not evidence.strip():
        raise ValueError('Promotion requires a passing scenario with an identified non-hypothesis expectation basis')
    if not scenario.get('expectedValue') and not result['checks']:
        raise ValueError('A regression needs an expectation')
    record = dict(schemaVersion=1, scenario=scenario, expectationBasis=scenario['basis'], evidence=evidence,
                  completionContract=result['completionContract'],
                  requiredInvariants=[check['invariant'] for check in result['checks']],
                  observed=dict(probeValue=result['probeValue'], framebufferHash=result['framebufferHash']),
                  validatedBuild=result['build'])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(record, indent=2) + '\n')
    return record


def main():
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest='command', required=True)
    run = commands.add_parser('run')
    run.add_argument('--repeat', type=int, default=2)
    sweep = commands.add_parser('sweep')
    for axis in ('phase', 'scx', 'wx', 'sprite-x', 'sprite-offset', 'model', 'speed'):
        sweep.add_argument('--' + axis)
    sweep.add_argument('--workers', type=int, default=2)
    sweep.add_argument('--limit', type=int, default=1024)
    reduction = commands.add_parser('reduce')
    reduction.add_argument('--max-trials', type=int, default=100)
    promotion = commands.add_parser('promote')
    promotion.add_argument('--evidence', required=True)
    for command in (run, sweep, reduction, promotion):
        command.add_argument('scenario', type=Path)
        command.add_argument('--binary', type=Path, default=DEFAULT_BINARY)
        command.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    scenario = json.loads(args.scenario.read_text())
    failed = False
    if args.command == 'run':
        if not 1 <= args.repeat <= 100:
            parser.error('Repeat must be 1..100')
        records = [execute(scenario, args.binary, args.output / str(index)) for index in range(args.repeat)]
        identical = all(equivalent(records[0], record) for record in records[1:])
        result = dict(schemaVersion=1, repeatedIdentically=identical, runs=records,
                      boundaries=boundary_observations(records[0], args.output / '0'))
        failed = not identical or records[0]['outcome'] != 'Pass'
    elif args.command == 'sweep':
        if not 1 <= args.workers <= 16 or not 1 <= args.limit <= 10000:
            parser.error('Workers must be 1..16 and limit 1..10000')
        axes = {}
        for argument, field in (('phase', 'phase'), ('scx', 'scx'), ('wx', 'wx'), ('sprite_x', 'spriteX'),
                                ('sprite_offset', 'spriteOffset'), ('model', 'model'), ('speed', 'doubleSpeed')):
            value = getattr(args, argument)
            if value is not None:
                axes[field] = value.split(',') if field == 'model' else [text == 'double' for text in value.split(
                    ',')] if field == 'doubleSpeed' else [int(text, 0) for text in value.split(',')]
                if field == 'doubleSpeed' and any(text not in ('regular', 'double') for text in value.split(',')):
                    parser.error('Speed values are regular,double')
        combinations = list(itertools.product(*axes.values()))
        if len(combinations) > args.limit:
            parser.error('Sweep exceeds the explicit case limit')

        def run_variant(index_and_values):
            index, values = index_and_values
            variant = scenario | dict(zip(axes, values))
            if variant.get('doubleSpeed') and not variant.get('model', 'DMGB').startswith(('CGB', 'AGB')):
                return dict(scenario=variant, outcome='UnsupportedConfiguration',
                            details='Double speed requires color hardware')
            directory = args.output / str(index)
            result = execute(variant, args.binary, directory)
            return dict(result=result, boundaries=boundary_observations(result, directory), directory=str(directory))

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as workers:
            records = list(workers.map(run_variant, enumerate(combinations)))
        result = dict(schemaVersion=1, axes=axes, runs=records)
        failed = any(record.get('result', {}).get('outcome', 'Pass') != 'Pass' for record in records)
    elif args.command == 'reduce':
        if not 1 <= args.max_trials <= 1000:
            parser.error('Reduction trials must be 1..1000')
        index = 0

        def evaluate(candidate):
            nonlocal index
            path = args.output / str(index)
            index += 1
            return execute(candidate, args.binary, path)

        result = reduce_scenario(scenario, evaluate, args.max_trials)
        (args.output / 'reduced-scenario.json').write_text(json.dumps(result['reducedScenario'], indent=2) + '\n')
    else:
        record = execute(scenario, args.binary, args.output.parent / (args.output.stem + '-validation'))
        promote(record['scenario'], record, args.evidence, args.output)
        print(json.dumps(dict(regression=str(args.output), outcome=record['outcome'])))
        return 0
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(report=str(args.output / 'report.json'), valid=not failed)))
    return int(failed)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f'Experiment infrastructure error: {error}') from error
