#!/usr/bin/env python3
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

spec = importlib.util.spec_from_file_location('execution_comparison', Path(__file__).with_name('compare-executions.py'))
execution = importlib.util.module_from_spec(spec)
spec.loader.exec_module(execution)


def read_capture(path):
    rows = [json.loads(line) for line in Path(str(path) + '.jsonl').read_text().splitlines() if line.strip()]
    trace = [json.loads(line) for line in Path(str(path) + '.events.jsonl').read_text().splitlines() if line.strip()]
    if (not rows or rows[0].get('record') != 'reference' or rows[0].get('schemaVersion') != 1
            or rows[-1].get('record') != 'complete' or rows[-1].get('samples') != len(rows) - 2):
        raise ValueError('Incomplete or unsupported reference observations')
    if not trace or trace[0].get('record') != 'trace' or trace[0].get('schemaVersion') != 1:
        raise ValueError('Unsupported reference trace')
    if trace[0]['overwritten'] or rows[-1]['overwrittenEvents']:
        raise ValueError('Reference history was overwritten; increase capacity or shorten the run')
    previous_request = previous_actual = 0
    for sample in rows[1:-1]:
        if (sample.get('record') != 'sample' or sample['actualTick'] < sample['requestedTick']
                or sample['overshootTicks'] != sample['actualTick'] - sample['requestedTick']
                or sample['requestedTick'] < previous_request or sample['actualTick'] < previous_actual):
            raise ValueError('Invalid reference timing or overshoot')
        previous_request, previous_actual = sample['requestedTick'], sample['actualTick']
    if rows[-1]['requestedTick'] != rows[0]['requestedTicks'] or rows[-1]['actualTick'] != previous_actual:
        raise ValueError('Reference capture did not reach its declared deadline')
    return dict(header=rows[0], samples=rows[1:-1], complete=rows[-1], events=trace[1:])


def compare_captures(left, right, anchor_pc=0x100, projection='instructions', address=None):
    a, b = left['header'], right['header']
    mismatches = [key for key in ('romHash', 'bootromHash', 'masterTicksPerSecond', 'initialCartridgeRam', 'rtc', 'requestedTicks') if a.get(key) != b.get(key)]
    mismatches.extend('settings.' + key for key in ('model', 'noBootrom') if a['settings'][key] != b['settings'][key])
    if mismatches:
        return dict(schemaVersion=1, status='Incompatible', mismatches=mismatches)
    capabilities = [a['capabilities'], b['capabilities']]
    result = dict(schemaVersion=1, status='Equal', projection=projection, capabilities=capabilities,
                  timingResolutionTicks=max(core['resolutionTicks'] for core in capabilities),
                  interpretation='A reference disagreement is evidence, not a hardware correctness verdict.',
                  limitations=[limitation for core in capabilities for limitation in core['limitations']])
    anchors = []
    for capture in (left, right):
        event = next((event for event in capture['events'] if event['kind'] == 'InstructionBegin' and event['address'] == anchor_pc), None)
        if not event:
            result.update(status='Incompatible', mismatches=[f'Alignment instruction ${anchor_pc:04X} was not captured'])
            return result
        anchors.append(event['position']['tick'])
    result['alignment'] = dict(kind='FirstInstructionAtAddress', address=anchor_pc, leftTick=anchors[0], rightTick=anchors[1],
                               phases=[core['instructionPhase'] for core in capabilities])
    end_tick = min(a['requestedTicks'] - anchors[0], b['requestedTicks'] - anchors[1])
    result['comparedThroughRelativeTick'] = end_tick
    if projection == 'state':
        indexed = [{sample['actualTick'] - anchor: sample for sample in capture['samples'] if anchor <= sample['actualTick'] <= anchor + end_tick}
                   for capture, anchor in zip((left, right), anchors)]
        common = sorted(indexed[0].keys() & indexed[1].keys())
        if not common:
            result.update(status='Incompatible', mismatches=['No samples at equal actual aligned times; requested times are never substituted'])
            return result
        result['comparedSamples'] = len(common)
        result['resolution'] = 'Only equal actual-time samples; operation overshoot may leave gaps'
        for tick in common:
            one, two = indexed[0][tick], indexed[1][tick]
            found = execution.difference({key: one[key] for key in ('registers', 'framebufferHash')},
                                         {key: two[key] for key in ('registers', 'framebufferHash')})
            if found:
                result.update(status='Divergence', firstRelativeTick=tick, difference=found, contextLeft=one, contextRight=two)
                break
        return result
    include_values = all(core['instructionValues' if projection == 'instructions' else 'busValues'] for core in capabilities)
    include_banks = all(core['banks'] for core in capabilities)
    streams = []
    for capture, anchor in zip((left, right), anchors):
        projected = []
        for event in capture['events']:
            tick = event['position']['tick'] - anchor
            if not 0 <= tick <= end_tick:
                continue
            if projection == 'instructions' and event['kind'] != 'InstructionBegin':
                continue
            if projection == 'bus' and (event['kind'] not in ('BusRead', 'BusWrite') or event['address'] != address):
                continue
            record = dict(position=dict(tick=tick), component=event['component'], kind=event['kind'], address=event['address'])
            if include_values:
                record['value'] = event['value']
            if include_banks:
                record['bank'] = event['bank'] if projection == 'bus' else event['pcBank']
            projected.append(record)
        streams.append(projected)
    result['comparedFields'] = ['tick', 'kind', 'address'] + (['value'] if include_values else []) + (['bank'] if include_banks else [])
    result['eventsLeft'], result['eventsRight'] = map(len, streams)
    if not any(streams):
        result.update(status='Incompatible', mismatches=['No observations match this projection'])
        return result
    found = execution.compare_events(*streams)
    if found:
        result.update(status='Divergence', firstRelativeTick=found['tick'], difference=found)
    return result


def run(args):
    args.output.mkdir(parents=True, exist_ok=True)
    isolated = args.output / 'inputs'
    isolated.mkdir(exist_ok=True)
    rom = isolated / ('rom' + args.rom.suffix)
    shutil.copyfile(args.rom, rom)
    captures = []
    for side in ('left', 'right'):
        binary = getattr(args, side).resolve()
        capabilities = json.loads(subprocess.check_output([str(binary), '--capabilities'], text=True))
        startup = 'SkipBootrom' if args.no_bootrom else 'ExplicitBootrom'
        if args.model not in capabilities['models'] or startup not in capabilities['startup']:
            return dict(schemaVersion=1, status='Incompatible', mismatches=[f'{side} does not support {args.model} with {startup}'], capabilities=capabilities)
        base = args.output / side
        command = [str(binary), str(rom.resolve()), '--model', args.model, '--ticks', str(args.ticks),
                   '--sample-ticks', str(args.sample_ticks), '--output', str(base.resolve())]
        if args.no_bootrom:
            command.append('--no-bootrom')
        else:
            command.extend(['--bios', str(args.bios.resolve())])
        process = subprocess.run(command, capture_output=True, text=True, check=False)
        base.with_suffix('.command.json').write_text(json.dumps(dict(command=command, returncode=process.returncode, stderr=process.stderr), indent=2) + '\n')
        if process.returncode:
            raise ValueError(f'{side} reference capture failed: {process.stderr}')
        captures.append(read_capture(base))
    return compare_captures(*captures, args.anchor_pc, args.projection, args.address)


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='command', required=True)
    saved = sub.add_parser('compare')
    saved.add_argument('left', type=Path)
    saved.add_argument('right', type=Path)
    launch = sub.add_parser('run')
    launch.add_argument('rom', type=Path)
    launch.add_argument('--left', type=Path, default=Path('build/release/Release/StarGBC_Reference'))
    launch.add_argument('--right', type=Path, required=True)
    launch.add_argument('--model', default='CGBC')
    startup = launch.add_mutually_exclusive_group(required=True)
    startup.add_argument('--bios', type=Path)
    startup.add_argument('--no-bootrom', action='store_true')
    launch.add_argument('--ticks', type=int, default=2106720)
    launch.add_argument('--sample-ticks', type=int, default=912)
    for command in (saved, launch):
        command.add_argument('--anchor-pc', type=lambda text: int(text, 0), default=0x100)
        command.add_argument('--projection', choices=('instructions', 'bus', 'state'), default='instructions')
        command.add_argument('--address', type=lambda text: int(text, 0))
        command.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.projection == 'bus' and args.address is None:
        parser.error('Bus comparison requires an explicit address; callback coverage differs across cores')
    if args.command == 'run':
        result = run(args)
        output = args.output / 'comparison.json'
    else:
        result = compare_captures(read_capture(args.left), read_capture(args.right), args.anchor_pc, args.projection, args.address)
        output = args.output
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(status=result['status'], report=str(output), firstRelativeTick=result.get('firstRelativeTick'))))
    return 0 if result['status'] == 'Equal' else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'Reference comparison infrastructure error: {error}') from error
