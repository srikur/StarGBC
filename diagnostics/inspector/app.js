'use strict';

const StarGBCInspector = (() => {
  const components = ['CPU', 'Bus', 'Interrupt', 'PPU', 'Timer', 'DMA', 'APU', 'Input', 'Control'];
  const kinds = ['InstructionBegin', 'InstructionEnd', 'BusRead', 'BusWrite', 'Halt', 'Stop', 'InterruptSample',
    'InterruptRequest', 'InterruptVisible', 'InterruptAccept', 'InterruptVector', 'RegisterWrite', 'RegisterApply',
    'PpuMode', 'WindowActivate', 'TileFetch', 'FifoPush', 'FifoPop', 'PixelDiscard', 'PixelStall', 'PixelOutput',
    'PixelRecolor', 'DmaOwnership', 'DmaTransfer', 'TimerEdge', 'TimerOverflow', 'TimerReload', 'AudioRegister',
    'AudioTrigger', 'AudioSequencer', 'AudioMix', 'InputApplied', 'Branch', 'Overflow', 'FramebufferClear'];
  const initiators = ['CPU', 'DMA', 'PPU', 'HDMA', 'Peripheral', 'Debugger', 'Replay'];
  const integer = value => BigInt(value ?? 0);
  const numberOrString = value => value <= BigInt(Number.MAX_SAFE_INTEGER) ? Number(value) : value.toString();
  const same = (left, right) => integer(left) === integer(right);

  function parseExactJson(text) {
    const pieces = [];
    let last = 0;
    for (let index = 0; index < text.length;) {
      if (text[index] === '"') {
        for (++index; index < text.length; ++index) {
          if (text[index] === '\\') ++index;
          else if (text[index] === '"') { ++index; break; }
        }
      } else if (text[index] === '-' || /[0-9]/.test(text[index])) {
        const begin = index++;
        while (index < text.length && /[0-9.eE+\-]/.test(text[index])) ++index;
        const token = text.slice(begin, index);
        if (/^-?\d+$/.test(token) && (BigInt(token) > BigInt(Number.MAX_SAFE_INTEGER) || BigInt(token) < BigInt(Number.MIN_SAFE_INTEGER))) {
          pieces.push(text.slice(last, begin), JSON.stringify(token));
          last = index;
        }
      } else ++index;
    }
    pieces.push(text.slice(last));
    return JSON.parse(pieces.join(''));
  }

  function parseTraceJson(text) {
    const rows = text.split(/\r?\n/).filter(line => line.trim()).map(parseExactJson);
    if (!rows.length || rows[0].record !== 'trace' || rows[0].schemaVersion !== 1)
      throw new Error('Unsupported JSONL trace schema');
    return { metadata: rows[0], events: rows.slice(1) };
  }

  function parseTraceBinary(buffer) {
    const view = new DataView(buffer);
    const decoder = new TextDecoder();
    if (view.byteLength < 24 || decoder.decode(new Uint8Array(buffer, 0, 8)) !== 'SGTRACE1' || view.getUint32(8, true) !== 1)
      throw new Error('Unsupported binary trace schema');
    const metadataLength = view.getUint32(12, true);
    const count = view.getBigUint64(16, true);
    if (count > 1000001n || BigInt(view.byteLength) !== 24n + BigInt(metadataLength) + count * 72n)
      throw new Error('Invalid binary trace length');
    const metadata = parseExactJson(decoder.decode(new Uint8Array(buffer, 24, metadataLength)));
    if (metadata.schemaVersion !== 1) throw new Error('Unsupported trace metadata');
    let offset = 24 + metadataLength;
    const read = bytes => {
      const value = bytes === 8 ? numberOrString(view.getBigUint64(offset, true)) : bytes === 4 ? view.getUint32(offset, true) :
        bytes === 2 ? view.getUint16(offset, true) : view.getUint8(offset);
      offset += bytes;
      return value;
    };
    const events = [];
    for (let index = 0; index < Number(count); ++index) {
      const position = { tick: read(8), frame: read(8), dot: read(2), ly: read(1), cpuPhase: read(1) };
      const event = { position, sequence: read(8), branch: read(8), pc: read(2), pcBank: read(2), address: read(2), bank: read(2),
        component: components[read(1)], kind: kinds[read(1)], initiator: initiators[read(1)], blocked: Boolean(read(1)),
        value: read(4), detail: read(4), relatedTick: read(8), parentBranch: read(8) };
      if (!event.component || !event.kind || !event.initiator) throw new Error('Unsupported binary event type');
      events.push(event);
    }
    return { metadata, events };
  }

  function selectPixel(provenance, x, y, frame = null, branch = null) {
    const desiredBranch = branch ?? provenance?.metadata?.branch ?? 0;
    const pixels = provenance?.pixels ?? [];
    for (let index = pixels.length - 1; index >= 0; --index) {
      const pixel = pixels[index];
      if (pixel.x === x && pixel.y === y && same(pixel.branch, desiredBranch) && (frame === null || same(pixel.emission.frame, frame)))
        return pixel;
    }
    return null;
  }

  function trackOf(event) {
    if (['TileFetch', 'WindowActivate', 'PpuMode', 'RegisterApply'].includes(event.kind)) return 1;
    if (['FifoPush', 'FifoPop', 'PixelDiscard', 'PixelStall', 'Pipeline'].includes(event.kind)) return 2;
    if (['PixelOutput', 'PixelRecolor', 'FramebufferClear'].includes(event.kind)) return 3;
    return 0;
  }

  function lineRecords(bundle, y, frame, branch) {
    const matches = position => position && position.ly === y && same(position.frame, frame);
    const records = (bundle.trace?.events ?? []).filter(event => same(event.branch, branch) && matches(event.position))
      .map(event => ({ ...event, source: 'trace', track: trackOf(event) }));
    const capture = bundle.provenance;
    for (const pixel of capture?.pixels ?? []) {
      if (pixel.y === y && same(pixel.emission.frame, frame) && same(pixel.branch, branch))
        records.push({ position: pixel.emission, branch: pixel.branch, kind: 'PixelOutput', address: y * 160 + pixel.x,
          value: pixel.emittedColor, source: 'provenance', track: 3, pixel });
    }
    for (const event of capture?.pipeline ?? [])
      if (event.y === y && matches(event.position) && same(event.branch, branch))
        records.push({ ...event, kind: 'Pipeline', source: 'provenance', track: 2 });
    for (const event of capture?.recolors ?? [])
      if (event.offset !== 65535 && Math.floor(event.offset / 160) === y && matches(event.position) && same(event.branch, branch))
        records.push({ ...event, kind: 'PixelRecolor', source: 'provenance', track: 3 });
    records.sort((left, right) => integer(left.position.tick) < integer(right.position.tick) ? -1 : integer(left.position.tick) > integer(right.position.tick) ? 1 : 0);
    return records;
  }

  function decodeBundle(texts, images = {}, binaryTrace = null) {
    const bundle = { images, trace: binaryTrace ? parseTraceBinary(binaryTrace) : null, provenance: null, result: null, difference: null };
    for (const [name, text] of Object.entries(texts)) {
      if (name.endsWith('.jsonl')) {
        if (name === 'events.jsonl') bundle.trace = parseTraceJson(text);
        continue;
      }
      if (!name.endsWith('.json')) continue;
      const value = parseExactJson(text);
      if (value.record === 'result' || value.record === 'experiment') bundle.result = value;
      else if (value.metadata?.record === 'pixel-provenance') bundle.provenance = value;
      else if ('mismatches' in value && 'first' in value) bundle.difference = value;
      else if (name === 'reproduction.json') bundle.reproduction = value;
      else if (name === 'terminal.json') bundle.terminal = value;
    }
    if (bundle.result && bundle.result.schemaVersion !== 1) throw new Error('Unsupported result schema');
    if (bundle.provenance && bundle.provenance.metadata.schemaVersion !== 1) throw new Error('Unsupported provenance schema');
    return bundle;
  }

  return { parseExactJson, parseTraceJson, parseTraceBinary, selectPixel, lineRecords, decodeBundle, integer, same };
})();

if (typeof module !== 'undefined') module.exports = StarGBCInspector;
if (typeof window !== 'undefined') window.StarGBCInspector = StarGBCInspector;

if (typeof document !== 'undefined') {
  const api = StarGBCInspector;
  const element = id => document.getElementById(id);
  const pretty = value => JSON.stringify(value, null, 2);
  const hex = (value, width = 4) => Number(value).toString(16).toUpperCase().padStart(width, '0');
  const descriptions = {
    SpriteTransparent: 'The background or window remains visible because the sprite FIFO has no opaque pixel here.',
    ObjectsDisabled: 'Object output was disabled by LCDC at this pixel.',
    BackgroundDisabled: 'The background and window were disabled, so the mixer emitted the backdrop.',
    BackgroundPriority: 'A nonzero background or window color and its priority attribute kept it in front of the sprite.',
    SpriteBehindBackground: 'The selected sprite was behind a nonzero background or window pixel.',
    SpriteForeground: 'The selected opaque sprite won the background comparison.',
    DisplayCleared: 'Disabling the LCD cleared this pixel after it was emitted.'
  };
  const state = { bundle: null, images: {}, urls: [], markers: [], pixel: null, records: [] };

  function status(text, error = false) {
    element('status').textContent = text;
    element('status').classList.toggle('error', error);
  }

  function selectOptions(id, values, initial, latest = false) {
    const select = element(id);
    select.replaceChildren();
    if (latest) select.add(new Option('Latest captured', 'latest'));
    for (const value of values) select.add(new Option(value, value));
    select.value = initial;
  }

  function choose(x, y) {
    element('pixel-x').value = Math.max(0, Math.min(159, Number(x) || 0));
    element('pixel-y').value = Math.max(0, Math.min(143, Number(y) || 0));
    updateSelection();
  }

  function drawImages(x, y) {
    for (const name of ['expected', 'actual', 'difference']) {
      const canvas = element(name + '-image');
      const context = canvas.getContext('2d');
      context.clearRect(0, 0, 160, 144);
      if (!state.images[name]) continue;
      context.drawImage(state.images[name], 0, 0, 160, 144);
      context.strokeStyle = '#e43384';
      context.lineWidth = .5;
      context.strokeRect(x - .25, y - .25, 1.5, 1.5);
    }
  }

  function facts(entries) {
    const target = element('pixel-facts');
    target.replaceChildren();
    for (const [label, value] of entries) {
      const term = document.createElement('dt'); term.textContent = label;
      const definition = document.createElement('dd'); definition.textContent = value;
      target.append(term, definition);
    }
  }

  function explain(pixel) {
    element('pixel-json').textContent = pixel ? pretty(pixel) : 'No pixel record in the selected capture.';
    const candidates = pixel?.sprites?.candidates.slice(0, pixel.sprites.count) ?? [];
    element('sprite-table').hidden = !candidates.length;
    element('sprite-note').hidden = Boolean(candidates.length);
    const body = element('sprite-table').querySelector('tbody'); body.replaceChildren();
    for (const candidate of candidates) {
      const row = document.createElement('tr');
      row.classList.toggle('winner', candidate.selected);
      for (const value of [candidate.origin.oamIndex, candidate.origin.color, candidate.decision, candidate.selected ? 'Yes' : 'No']) {
        const cell = document.createElement('td'); cell.textContent = value; row.append(cell);
      }
      body.append(row);
    }
    if (!pixel) {
      const region = state.bundle?.provenance?.metadata.region;
      element('pixel-explanation').textContent = region ? `No provenance for this pixel in the selected frame and branch. Captured lines: ${region.y.first}–${region.y.last}.` : 'This bundle has no pixel provenance. Capture the relevant scanline to inspect its decisions.';
      facts([]); return;
    }
    const source = pixel.selected;
    const winner = source.source === 'Sprite' ? `Sprite OAM ${source.oamIndex} supplies color ${source.color}. ` : `${source.source} supplies color ${source.color}. `;
    element('pixel-explanation').textContent = winner + (descriptions[pixel.decision] ?? pixel.decision) +
      (!source.valid ? ' Historical fetch data is unavailable for this in-flight pixel.' : '') +
      (pixel.recolored ? ` Its color changed after emission, at tick ${pixel.finalColorTick}.` : '');
    const entries = [['Frame / branch', `${pixel.emission.frame} / ${pixel.branch}`], ['Output tick / dot', `${pixel.emission.tick} / ${pixel.emission.dot}`],
      ['Emitted color', '#' + hex(pixel.emittedColor, 8)], ['Final color', '#' + hex(pixel.finalColor, 8)]];
    if (source.valid && ['Background', 'Window', 'Sprite'].includes(source.source)) {
      entries.push(['Map / OAM', '$' + hex(source.mapAddress)], ['Tile / bank', `${source.tile} / ${source.bank}`],
        ['Low bitplane', `$${hex(source.lowAddress)} = $${hex(source.lowRead, 2)} at tick ${source.lowTick}`],
        ['High bitplane', `$${hex(source.highAddress)} = $${hex(source.highRead, 2)} at tick ${source.highTick}`],
        ['Row / bit', `${source.lowRow}, ${source.highRow} / ${source.bit}`], ['Palette', source.palette]);
    }
    facts(entries);
  }

  function drawTimeline() {
    const canvas = element('timeline');
    const context = canvas.getContext('2d');
    const colors = ['#a2bafa', '#f5c176', '#c49df4', '#71e0bb'];
    context.clearRect(0, 0, canvas.width, canvas.height);
    context.font = '13px system-ui';
    state.markers = [];
    const records = state.records;
    for (let track = 0; track < 4; ++track) {
      const y = 58 + track * 46;
      context.fillStyle = '#b8c7d5'; context.fillText(['CPU / bus', 'Fetch / mode', 'FIFO / discard', 'Pixels'][track], 16, y + 5);
      context.strokeStyle = '#30404f'; context.beginPath(); context.moveTo(130, y); context.lineTo(1080, y); context.stroke();
    }
    if (!records.length) return;
    const minimum = api.integer(records[0].position.tick);
    const maximum = api.integer(records.at(-1).position.tick) + 1n;
    const zoom = Number(element('zoom').value);
    const fullSpan = maximum - minimum;
    const span = fullSpan / BigInt(zoom) || 1n;
    const focus = api.integer(state.pixel?.emission.tick ?? records[0].position.tick);
    let begin = focus - span / 2n;
    if (begin < minimum) begin = minimum;
    if (begin + span > maximum) begin = maximum - span;
    const project = tick => 130 + Number(api.integer(tick) - begin) / Number(span) * 950;
    context.fillStyle = '#a6b6c5'; context.font = '11px ui-monospace, monospace';
    for (let index = 0; index <= 4; ++index) {
      const tick = begin + span * BigInt(index) / 4n;
      context.textAlign = index === 4 ? 'right' : 'left'; context.fillText(tick.toString(), 130 + index * 237.5, 23);
    }
    context.textAlign = 'left';
    for (const event of records) {
      const x = project(event.position.tick); if (x < 130 || x > 1080) continue;
      const y = 58 + event.track * 46;
      context.fillStyle = colors[event.track];
      context.globalAlpha = event.kind.startsWith('Instruction') ? .35 : .85;
      context.fillRect(x - 1.5, y - 10, 3, 20);
      state.markers.push({ x, y, event });
    }
    context.globalAlpha = 1;
    if (state.pixel) {
      const x = project(state.pixel.emission.tick);
      if (x >= 130 && x <= 1080) {
        context.strokeStyle = '#ffffff'; context.setLineDash([3, 4]); context.beginPath(); context.moveTo(x, 35); context.lineTo(x, 220); context.stroke(); context.setLineDash([]);
      }
    }
  }

  function updateSelection() {
    const x = Number(element('pixel-x').value), y = Number(element('pixel-y').value);
    const requestedFrame = element('frame').value === 'latest' ? null : element('frame').value;
    const branch = element('branch').value || '0';
    const pixel = api.selectPixel(state.bundle?.provenance, x, y, requestedFrame, branch);
    state.pixel = pixel;
    element('pixel-heading').textContent = `Pixel (${x}, ${y})`;
    element('timeline-heading').textContent = `Scanline ${y}`;
    drawImages(x, y); explain(pixel);
    const matchingTrace = (state.bundle?.trace?.events ?? []).filter(event => event.position?.ly === y && api.same(event.branch, branch));
    const frame = requestedFrame ?? pixel?.emission.frame ?? matchingTrace.at(-1)?.position.frame ?? 0;
    state.records = state.bundle ? api.lineRecords(state.bundle, y, frame, branch) : [];
    element('timeline-note').textContent = state.records.length ? `${state.records.length} captured records · frame ${frame} · branch ${branch}. Click a marker for its exact timestamp and fields.` : 'No events were captured for this scanline, frame and branch.';
    element('zoom-value').textContent = element('zoom').value + '×';
    drawTimeline();
  }

  async function loadBundle(bundle) {
    state.bundle = bundle; state.images = {};
    const result = bundle.result;
    element('case-summary').hidden = !result;
    if (result) {
      element('outcome').textContent = result.outcome;
      if (result.record === 'experiment') {
        element('case-name').textContent = `${result.scenario.mode} ${result.scenario.family} · ${result.scenario.model}`;
        element('case-details').textContent = result.failureSignature || result.completionContract;
        element('expected-value').textContent = result.scenario.expectedValue ?? 'Recorded event invariants';
        element('actual-value').textContent = result.probeValue;
      } else {
        const identity = result.test.id.split('|');
        element('case-name').textContent = `${identity.length === 5 ? identity[1] : result.test.rom} · ${result.test.model}`;
        element('case-details').textContent = result.details;
        element('expected-value').textContent = result.expected;
        element('actual-value').textContent = result.actual;
      }
    }
    await Promise.all(['expected', 'actual', 'difference'].map(async name => {
      const source = bundle.images[name + '.bmp'];
      if (!source) { element(name + '-image-note').textContent = 'No image in this bundle'; return; }
      const image = new Image(); image.src = source; await image.decode();
      if (image.naturalWidth !== 160 || image.naturalHeight !== 144) throw new Error('Image dimensions must be 160 × 144');
      state.images[name] = image; element(name + '-image-note').textContent = 'Click a pixel · arrow keys move selection';
    }));
    const pixels = bundle.provenance?.pixels ?? [];
    const positions = [...pixels.map(pixel => ({ frame: pixel.emission.frame, branch: pixel.branch })),
      ...(bundle.trace?.events ?? []).filter(event => event.position.ly < 144).map(event => ({ frame: event.position.frame, branch: event.branch }))];
    const ordered = values => [...new Set(values.map(String))].sort((left, right) => api.integer(left) < api.integer(right) ? -1 : api.integer(left) > api.integer(right) ? 1 : 0);
    selectOptions('frame', ordered(positions.map(position => position.frame)), 'latest', true);
    const branches = ordered(positions.map(position => position.branch));
    selectOptions('branch', branches.length ? branches : ['0'], String(bundle.provenance?.metadata.branch ?? branches.at(-1) ?? 0));
    element('first-difference').disabled = !bundle.difference?.first;
    element('mismatch-summary').textContent = bundle.difference ? `${bundle.difference.mismatches} differing pixels` : 'No pixel comparison metadata';
    element('capture-details').textContent = pretty({ provenance: bundle.provenance?.metadata, trace: bundle.trace?.metadata,
      initialInputs: result?.inputs, normalization: bundle.reproduction?.normalization, terminal: bundle.terminal });
    const truncation = (bundle.trace?.metadata.overwritten ?? 0) !== 0 || (bundle.provenance?.metadata.overwrittenPixels ?? 0) !== 0 || (bundle.provenance?.metadata.overwrittenEvents ?? 0) !== 0;
    status(`Loaded ${pixels.length} pixel explanations and ${bundle.trace?.events.length ?? 0} trace events.${truncation ? ' Earlier records were overwritten; see capture details.' : ''}`);
    const first = bundle.difference?.first ?? pixels[0] ?? { x: 0, y: 0 }; choose(first.x, first.y);
  }

  async function openFiles(files) {
    for (const url of state.urls) URL.revokeObjectURL(url); state.urls = [];
    const named = [...files].map(file => ({ file, path: file.webkitRelativePath || file.name }));
    const roots = named.filter(entry => entry.path.endsWith('/result.json')).sort((left, right) => left.path.length - right.path.length);
    const root = roots.length ? roots[0].path.slice(0, -'result.json'.length) : '';
    const selected = named.filter(entry => !root || entry.path.startsWith(root) && !entry.path.slice(root.length).includes('/'));
    const texts = {}, images = {}; let binary = null;
    for (const { file, path } of selected) {
      const name = path.split('/').at(-1);
      if (/\.jsonl?$/.test(name)) texts[name] = await file.text();
      else if (['expected.bmp', 'actual.bmp', 'difference.bmp'].includes(name)) { const url = URL.createObjectURL(file); state.urls.push(url); images[name] = url; }
      else if (name === 'events.trace') binary = await file.arrayBuffer();
    }
    if (!Object.keys(texts).length && !binary) throw new Error('No diagnostics JSON or trace files found');
    await loadBundle(api.decodeBundle(texts, images, binary));
  }

  const attempt = action => Promise.resolve().then(action).catch(error => status(error.message, true));
  element('bundle-files').addEventListener('change', event => attempt(() => openFiles(event.target.files)));
  document.addEventListener('dragover', event => { event.preventDefault(); document.body.classList.add('dragging'); });
  document.addEventListener('dragleave', event => { if (!event.relatedTarget) document.body.classList.remove('dragging'); });
  document.addEventListener('drop', event => { event.preventDefault(); document.body.classList.remove('dragging'); attempt(() => openFiles(event.dataTransfer.files)); });
  for (const name of ['expected', 'actual', 'difference']) {
    const canvas = element(name + '-image');
    canvas.addEventListener('click', event => { const rect = canvas.getBoundingClientRect(); choose(Math.floor((event.clientX - rect.left) * 160 / rect.width), Math.floor((event.clientY - rect.top) * 144 / rect.height)); });
    canvas.addEventListener('keydown', event => { const moves = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }; if (moves[event.key]) { event.preventDefault(); choose(Number(element('pixel-x').value) + moves[event.key][0], Number(element('pixel-y').value) + moves[event.key][1]); } });
  }
  for (const id of ['pixel-x', 'pixel-y']) element(id).addEventListener('change', () => choose(element('pixel-x').value, element('pixel-y').value));
  for (const id of ['frame', 'branch', 'zoom']) element(id).addEventListener('input', updateSelection);
  element('first-difference').addEventListener('click', () => { const first = state.bundle?.difference?.first; if (first) choose(first.x, first.y); });
  element('timeline').addEventListener('click', event => {
    const rect = event.target.getBoundingClientRect(); const x = (event.clientX - rect.left) * 1100 / rect.width, y = (event.clientY - rect.top) * 250 / rect.height;
    const nearby = state.markers.filter(marker => Math.abs(marker.y - y) < 20).sort((left, right) => Math.abs(left.x - x) - Math.abs(right.x - x))[0];
    if (!nearby || Math.abs(nearby.x - x) > 12) return;
    element('event-title').textContent = `${nearby.event.kind} · tick ${nearby.event.position.tick}`;
    element('event-json').textContent = pretty(nearby.event);
    if (nearby.event.kind === 'PixelOutput') choose(nearby.event.address % 160, Math.floor(nearby.event.address / 160));
  });
  const embedded = element('embedded-bundle').textContent.trim();
  if (embedded) attempt(() => { const data = api.parseExactJson(embedded); return loadBundle(api.decodeBundle(data.texts, data.images)); });
  else updateSelection();
}
