'use strict';
const assert = require('node:assert/strict');
const {test} = require('node:test');
const fs = require('node:fs');
const inspector = require('../../diagnostics/inspector/app.js');

const position = (tick, frame = 3, ly = 10) => ({tick, frame, dot: 90, ly, cpuPhase: 2});
const pixel = (tick, branch = 0) => ({x: 17, y: 10, branch, emission: position(tick), emittedColor: 42});

test('64-bit timestamps and sentinels survive JSON parsing without rounding', () => {
    const parsed = inspector.parseExactJson('{"tick":9007199254740993,"unknown":18446744073709551615,"negative":-9007199254740993,"text":"18446744073709551615 \\\" ","small":42,"float":1.25e2}');
    assert.equal(parsed.tick, '9007199254740993');
    assert.equal(parsed.unknown, '18446744073709551615');
    assert.equal(parsed.negative, '-9007199254740993');
    assert.equal(parsed.small, 42);
    assert.equal(parsed.float, 125);
});

test('pixel selection honors replay branches and explicit frames', () => {
    const provenance = {
        metadata: {branch: 1},
        pixels: [pixel(10), pixel(20, 1), {...pixel(30, 1), emission: position(30, 4)}]
    };
    assert.equal(inspector.selectPixel(provenance, 17, 10).emission.tick, 30);
    assert.equal(inspector.selectPixel(provenance, 17, 10, 3).emission.tick, 20);
    assert.equal(inspector.selectPixel(provenance, 17, 10, 3, 0).emission.tick, 10);
    assert.equal(inspector.selectPixel(provenance, 18, 10), null);
});

test('scanline tracks retain chronological historical records in the selected branch', () => {
    const bundle = {
        provenance: {
            pixels: [pixel('9007199254740995')],
            pipeline: [{position: position('9007199254740993'), branch: 0, y: 10, reason: 'WindowClipping'}],
            recolors: [{position: position('9007199254740996'), branch: 0, offset: 1617}]
        }, trace: {
            events: [
                {position: position('9007199254740994'), branch: 0, kind: 'BusRead'},
                {position: position(1), branch: 1, kind: 'BusRead'},
                {position: position(2, 4), branch: 0, kind: 'BusRead'}]
        }
    };
    const events = inspector.lineRecords(bundle, 10, 3, 0);
    assert.deepEqual(events.map(event => event.kind), ['Pipeline', 'BusRead', 'PixelOutput', 'PixelRecolor']);
    assert.deepEqual(events.map(event => event.track), [2, 0, 3, 3]);
});

test('binary and JSON traces describe the same exact event', () => {
    const metadata = {record: 'trace', schemaVersion: 1, overwritten: 5};
    const header = Buffer.from(JSON.stringify(metadata));
    const buffer = Buffer.alloc(24 + header.length + 72);
    buffer.write('SGTRACE1');
    buffer.writeUInt32LE(1, 8);
    buffer.writeUInt32LE(header.length, 12);
    buffer.writeBigUInt64LE(1n, 16);
    header.copy(buffer, 24);
    let offset = 24 + header.length;
    for (const [bytes, value] of [[8, 9007199254740993n], [8, 3], [2, 90], [1, 10], [1, 2], [8, 8], [8, 0],
        [2, 256], [2, 2], [2, 65345], [2, 0], [1, 1], [1, 2], [1, 0], [1, 1], [4, 194], [4, 7], [8, 1], [8, 0]]) {
        if (bytes === 8) buffer.writeBigUInt64LE(BigInt(value), offset);
        else buffer.writeUIntLE(value, offset, bytes);
        offset += bytes;
    }
    const event = {
        position: position('9007199254740993'),
        sequence: 8,
        branch: 0,
        pc: 256,
        pcBank: 2,
        address: 65345,
        bank: 0,
        component: 'Bus',
        kind: 'BusRead',
        initiator: 'CPU',
        blocked: true,
        value: 194,
        detail: 7,
        relatedTick: 1,
        parentBranch: 0
    };
    const parsed = inspector.parseTraceBinary(buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength));
    assert.deepEqual(parsed, inspector.parseTraceJson(JSON.stringify(metadata) + '\n' + JSON.stringify(event)));
    assert.throws(() => inspector.parseTraceBinary(buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength - 1)), /length/);
    assert.throws(() => inspector.parseTraceJson('{"record":"trace","schemaVersion":2}'), /schema/);
});

if (process.env.STARGBC_INSPECTOR_BUNDLE) test('captured binary trace agrees with the JSON trace', () => {
    const directory = process.env.STARGBC_INSPECTOR_BUNDLE;
    const buffer = fs.readFileSync(directory + '/events.trace');
    const binary = inspector.parseTraceBinary(buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength));
    const json = inspector.parseTraceJson(fs.readFileSync(directory + '/events.jsonl', 'utf8'));
    assert.deepEqual(binary, json);
});

test('experiment reports share the pixel and trace bundle decoder', () => {
    const result = {
        record: 'experiment',
        schemaVersion: 1,
        outcome: 'Pass',
        scenario: {mode: 'WholeMachine', family: 'Sprites', model: 'CGBC'}
    };
    const bundle = inspector.decodeBundle({'result.json': JSON.stringify(result)});
    assert.deepEqual(bundle.result, result);
    assert.equal(bundle.difference, null);
});
