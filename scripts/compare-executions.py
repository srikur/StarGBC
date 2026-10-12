#!/usr/bin/env python3
import argparse
import copy
import json
from pathlib import Path
import subprocess


def read_observations(path):
    rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    if not rows or rows[0].get('record') != 'observations' or rows[0].get('schemaVersion') != 1:
        raise ValueError(f'Unsupported observation stream: {path}')
    header, samples = rows[0], rows[1:-1]
    if rows[-1].get('record') != 'complete' or rows[-1].get('samples') != len(samples):
        raise ValueError(f'Incomplete observation stream: {path}')
    ticks = [sample['state']['position']['tick'] for sample in samples]
    if (not ticks or any(sample.get('record') != 'sample' for sample in samples)
            or any(right <= left for left, right in zip(ticks, ticks[1:]))
            or ticks[0] != header['plan']['beginTick'] or ticks[-1] != header['plan']['endTick']
            or rows[-1]['tick'] != ticks[-1]):
        raise ValueError(f'Invalid ordered observation boundaries: {path}')
    return header, samples


def difference(left, right, path=''):
    if type(left) is not type(right):
        return dict(field=path, left=left, right=right)
    if isinstance(left, dict):
        for key in sorted(left.keys() | right.keys()):
            location = f'{path}.{key}' if path else key
            if key not in left or key not in right:
                return dict(field=location, left=left.get(key), right=right.get(key))
            found = difference(left[key], right[key], location)
            if found:
                return found
    elif isinstance(left, list):
        for index, (a, b) in enumerate(zip(left, right)):
            found = difference(a, b, f'{path}[{index}]')
            if found:
                return found
        if len(left) != len(right):
            return dict(field=path + '.length', left=len(left), right=len(right))
    elif left != right:
        return dict(field=path, left=left, right=right)
    return None


def event_payload(event, timing=True):
    value = copy.deepcopy(event)
    value.pop('sequence', None)
    if not timing:
        value.pop('position', None)
    return value


def compare_events(left, right):
    left = sorted(left, key=lambda event: (event['position']['tick'], event['component']))
    right = sorted(right, key=lambda event: (event['position']['tick'], event['component']))
    for index in range(max(len(left), len(right))):
        a = left[index] if index < len(left) else None
        b = right[index] if index < len(right) else None
        if a and b and event_payload(a) == event_payload(b):
            continue
        kind = 'ChangedEvent'
        if a is None or (index + 1 < len(right) and event_payload(a) == event_payload(right[index + 1])):
            kind = 'MissingFromLeft'
        elif b is None or (index + 1 < len(left) and event_payload(left[index + 1]) == event_payload(b)):
            kind = 'ExtraOnLeft'
        elif event_payload(a, False) == event_payload(b, False):
            kind = 'Mistimed'
        elif all(a.get(key) == b.get(key) for key in ('component', 'kind', 'address', 'pc', 'pcBank')):
            kind = 'ChangedValue'
        return dict(kind=kind, index=index, tick=min(event['position']['tick'] for event in (a, b) if event),
                    left=a, right=b, contextLeft=left[max(0, index - 3):index + 4], contextRight=right[max(0, index - 3):index + 4])
    return None


def canonical_sample(sample, hardware):
    value = copy.deepcopy(sample)
    value.pop('events', None)
    value.pop('record', None)
    if not hardware:
        value['state'].pop('hardwareHash', None)
    value['state'].get('machine', {}).get('audio', {}).pop('mixerLevelChanges', None)
    return value


def compare_observations(left, right):
    a, left_samples = left
    b, right_samples = right
    compatibility = ('schemaVersion', 'initial', 'replayHash', 'masterTicksPerSecond')
    mismatches = [key for key in compatibility if a.get(key) != b.get(key)]
    mismatches.extend('plan.' + key for key in ('beginTick', 'endTick', 'sampleTicks', 'events') if a['plan'].get(key) != b['plan'].get(key))
    if mismatches:
        return dict(schemaVersion=1, status='Incompatible', mismatches=mismatches)
    hardware = a['build'] == b['build'] and a['hardwareFormat'] == b['hardwareFormat']
    report = dict(schemaVersion=1, status='Equal', startTick=a['plan']['beginTick'], endTick=a['plan']['endTick'],
                  resolutionTicks=a['plan']['sampleTicks'], hardwareCompared=hardware, eventsCompared=a['plan']['events'],
                  comparedSamples=0, limitations=['Equality applies to the recorded observations and their sampling resolution.'])
    if not hardware:
        report['limitations'].append('Private serialized state is compared only for the same core build and format; cross-build records use versioned observations.')
    previous = a['plan']['beginTick']
    for index in range(max(len(left_samples), len(right_samples))):
        if index >= min(len(left_samples), len(right_samples)):
            report.update(status='Incompatible', mismatches=['sample count'])
            return report
        one, two = left_samples[index], right_samples[index]
        found = difference(canonical_sample(one, hardware), canonical_sample(two, hardware))
        report['comparedSamples'] += 1
        if found:
            report.update(status='Divergence', lowerTick=previous, upperTick=min(one['state']['position']['tick'], two['state']['position']['tick']),
                          difference=found, contextLeft=left_samples[max(0, index - 1):index + 2], contextRight=right_samples[max(0, index - 1):index + 2])
            if one.get('events') is not None and two.get('events') is not None:
                report['eventDifference'] = compare_events(one['events'], two['events'])
            return report
        previous = one['state']['position']['tick']
    return report


def capture(args, side, phase, begin, end, sample_ticks, events=False, details=False):
    binary = getattr(args, side).resolve()
    path = args.output / f'{phase}-{side}.jsonl'
    command = [str(binary), str(args.rom.resolve()), '--model', args.model, '--policy', getattr(args, side + '_policy'),
               '--comparison-output', str(path.resolve()), '--comparison-begin', str(begin), '--comparison-end', str(end),
               '--sample-ticks', str(sample_ticks), '--output', str(path.with_suffix('.terminal.json').resolve())]
    if args.no_bootrom:
        command.append('--no-bootrom')
    if args.raw_color:
        command.append('--raw-color')
    if args.replay_inputs:
        command.extend(['--replay-inputs', str(args.replay_inputs.resolve())])
    if events:
        command.append('--comparison-events')
    if details:
        command.append('--comparison-event-details')
    process = subprocess.run(command, capture_output=True, text=True, check=False)
    path.with_suffix('.command.json').write_text(json.dumps(dict(command=command, returncode=process.returncode, stderr=process.stderr), indent=2) + '\n')
    if process.returncode:
        raise ValueError(f'{side} capture failed: {process.stderr}')
    return read_observations(path)


def run_comparison(args):
    args.output.mkdir(parents=True, exist_ok=True)
    reports = []
    for phase, events in (('policy', False), ('events', True)):
        pair = [capture(args, side, phase, 0, args.ticks, args.sample_ticks, events) for side in ('left', 'right')]
        report = compare_observations(*pair)
        report['phase'] = phase
        reports.append(report)
        if report['status'] == 'Incompatible':
            break
        if report['status'] == 'Divergence':
            refined_pair = [capture(args, side, phase + '-refined', report['lowerTick'], report['upperTick'], 1, events, events) for side in ('left', 'right')]
            report['refined'] = compare_observations(*refined_pair)
    incompatible = next((report for report in reports if report['status'] == 'Incompatible'), None)
    divergences = [report for report in reports if report['status'] == 'Divergence']
    result = dict(schemaVersion=1, status='Incompatible' if incompatible else 'Divergence' if divergences else 'Equal', phases=reports,
                  checkpointPolicy='Each process restarts from verified initial conditions; no checkpoint crosses builds.',
                  eventScheduling='Event capture uses per-tick CPU scheduling; the separate policy phase exercises unobserved scheduler batching.')
    if divergences:
        result['earliest'] = min(divergences, key=lambda report: report.get('refined', report).get('upperTick', report['upperTick']))
    (args.output / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='command', required=True)
    compare = sub.add_parser('compare')
    compare.add_argument('left', type=Path)
    compare.add_argument('right', type=Path)
    compare.add_argument('--output', type=Path, required=True)
    run = sub.add_parser('run')
    run.add_argument('rom', type=Path)
    run.add_argument('--left', type=Path, default=Path('build/release/Release/StarGBC_Debug'))
    run.add_argument('--right', type=Path, default=Path('build/release/Release/StarGBC_Debug'))
    run.add_argument('--left-policy', choices=('Optimized', 'Ordinary'), default='Optimized')
    run.add_argument('--right-policy', choices=('Optimized', 'Ordinary'), default='Ordinary')
    run.add_argument('--model', default='CGBC')
    run.add_argument('--no-bootrom', action='store_true')
    run.add_argument('--raw-color', action='store_true')
    run.add_argument('--replay-inputs', type=Path)
    run.add_argument('--ticks', type=int, default=2106720)
    run.add_argument('--sample-ticks', type=int, default=912)
    run.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'run':
        if args.ticks < 0 or not 1 <= args.sample_ticks <= 140448:
            parser.error('Ticks must be nonnegative and samples must span 1..140448 master ticks')
        result = run_comparison(args)
    else:
        result = compare_observations(read_observations(args.left), read_observations(args.right))
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key not in ('phases', 'contextLeft', 'contextRight', 'earliest')}))
    return 0 if result['status'] == 'Equal' else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f'Differential infrastructure error: {error}') from error
