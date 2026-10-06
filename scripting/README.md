# StarGBC Lua API

StarGBC provides the global `stargbc` table to scripts running in Lua **5.5.1**. The current API version is **1**. Each
emulator instance runs one script environment at a time.

```lua
assert(stargbc.api_version == 1)

stargbc.on("before_frame", function(frame)
    -- Hold A for three frames, then release it for three frames.
    -- Other buttons follow physical input.
    stargbc.joypad.set({A = (frame - 1) % 6 < 3})
end)

stargbc.on("after_frame", function(frame)
    if frame % 60 == 0 then
        local value = stargbc.memory.peek8(0xC000)
        stargbc.log(string.format("Frame %d: C000 = %02X", frame, value))
    end
end)
```

Functions documented without a return value return no Lua values. Integer parameters must be Lua integers in the stated
range; numeric strings and floating-point values such as `1.0` are rejected. Event names and button names are
case-sensitive. Invalid arguments and unsupported operations raise Lua errors.

## Execution and callbacks

The script's top-level code executes once when the environment starts. Use it to initialize variables and register
callbacks. Emulation advances between callbacks; scripts cannot step the emulator themselves or call a `frameadvance()`
function.

Starting or reloading creates a fresh environment. Script variables, callbacks, and snapshots from the previous
environment are discarded. Starting a script does not load a game checkpoint or emit `state_loaded`; callbacks initially
observe the emulator's current state.

### `stargbc.on(event, callback)`

Registers a function for an event. **Only allowed during top-level initialization.** Each event has one callback;
registering the same event again replaces its previous callback. Passing `nil` to unregister a callback is unsupported.
Callback return values are ignored.

| Event            | Callback signature | When it runs                                                                                                       |
|------------------|--------------------|--------------------------------------------------------------------------------------------------------------------|
| `"before_frame"` | `function(frame)`  | Before script input is applied and before the upcoming emulation interval.                                         |
| `"after_frame"`  | `function(frame)`  | After that interval completes, before frontend presentation. Receives the same frame number as its `before_frame`. |
| `"state_loaded"` | `function()`       | After a successful checkpoint restore, including a restore requested by the script. Receives no arguments.         |

A frame is one fixed emulation interval: 140,448 master cycles, equivalent to 70,224 normal-speed Game Boy cycles.
Callbacks run independently of display refresh, emulation speed, and whether the emulated LCD is enabled.

Frame numbers start at 1 for a new emulation session. A script started later receives the session's current frame
numbers. They do not reset when the script restarts or a checkpoint is restored.

Frame callbacks do not run while emulation is paused. `state_loaded` can run while paused. Failed restores do not emit
an event. A restore or pause requested in `before_frame` can prevent the upcoming interval and its `after_frame`;
see [snapshots](#snapshots) and [execution control](#execution-control).

### When functions are allowed

"Any" means top-level initialization or any of the three event callbacks.

| Operation                                                 | Allowed context                        |
|-----------------------------------------------------------|----------------------------------------|
| `stargbc.on`                                              | Initialization only                    |
| Logging, emulator information, memory reads, `joypad.get` | Any                                    |
| `memory.poke8`, `joypad.set`                              | `before_frame` only                    |
| `state.save`, `pause`                                     | Any                                    |
| `state.load`                                              | `before_frame` or `after_frame`        |
| `stop`                                                    | Any event callback; not initialization |

## Emulator information and logging

| Value or function       | Result      | Meaning                                                                                                                                         |
|-------------------------|-------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| `stargbc.api_version`   | Integer `1` | Version of the emulator scripting API.                                                                                                          |
| `stargbc.frame_count()` | Integer     | Number of completed emulation intervals in this session. In `before_frame(frame)`, this is `frame - 1`; in `after_frame(frame)`, it is `frame`. |
| `stargbc.model()`       | String      | Canonical lowercase hardware model, such as `"dmgb"`, `"cgbe"`, or `"agbb"`.                                                                    |
| `stargbc.is_cgb_mode()` | Boolean     | Whether the emulator is currently operating in Game Boy Color mode. Color-capable hardware can also run in monochrome mode.                     |
| `stargbc.log(message)`  | —           | Logs a string, labelled with the script's source name.                                                                                          |
| `print(...)`            | —           | Logs its arguments after string conversion, separated by tabs. Uses the same output limits as `stargbc.log`.                                    |

The frame counter measures emulated intervals, not wall-clock time or frames since script activation. To count from
script activation, maintain a local counter in a callback.

## Memory

Memory reads inspect storage without changing emulated state or advancing emulated time. Reads and writes bypass
temporary CPU access restrictions caused by graphics activity or DMA. Addresses in this section are hexadecimal.

### Read functions

| Function                                 | Parameters                                                     | Result                                                                                  |
|------------------------------------------|----------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| `stargbc.memory.peek8(address)`          | Integer address `0x0000`–`0xFFFF` in a supported region        | Integer byte `0`–`255` from the currently mapped address.                               |
| `stargbc.memory.peek16_le(address)`      | Integer address `0x0000`–`0xFFFE`; both bytes must be readable | Integer `0`–`65535`, calculated as `peek8(address) + 256 * peek8(address + 1)`.         |
| `stargbc.memory.peek_wram(bank, offset)` | Integer bank `0`–`7`; integer offset `0x000`–`0xFFF`           | Integer byte `0`–`255` from a physical work RAM bank, independent of the selected bank. |

`peek16_le` does not wrap at the end of the address space. For example, `peek16_le(0xFFFE)` raises an error because its
second byte would be the unsupported interrupt-enable register at `0xFFFF`.

For `peek_wram`, bank 0 is the fixed work RAM bank normally mapped at `0xC000`–`0xCFFF`. Banks 1–7 are the banks
selectable at `0xD000`–`0xDFFF`. Only banks 0 and 1 are accepted outside CGB mode. The offset is relative to the start
of the bank, not a CPU address.

```lua
local mapped = stargbc.memory.peek8(0xD123)
local bank_one = stargbc.memory.peek_wram(1, 0x123)
-- These can differ when another work RAM bank is selected.
```

### `stargbc.memory.poke8(address, value)`

Patches one byte of supported RAM. **Only allowed in `before_frame`.** `address` must be an integer in `0x0000`–
`0xFFFF`, and `value` must be an integer in `0`–`255`.

The write takes effect immediately, so a subsequent read in the same callback sees it. It does not advance emulated
time. There is no physical-bank write function: banked writes use the currently selected bank. Cartridge RAM writes mark
the battery save dirty for the emulator's normal save handling.

```lua
stargbc.on("before_frame", function()
    stargbc.memory.poke8(0xC010, 0x2A)
    assert(stargbc.memory.peek8(0xC010) == 0x2A)
end)
```

### Supported address regions

| Address range                        | Reads                                                                 | Writes                            |
|--------------------------------------|-----------------------------------------------------------------------|-----------------------------------|
| `0000`–`7FFF`                        | Currently mapped cartridge ROM or boot ROM overlay                    | Unsupported                       |
| `8000`–`9FFF`                        | Selected video RAM bank                                               | Patch selected bank               |
| `A000`–`BFFF`                        | Selected enabled cartridge RAM; `FF` if absent, disabled, or unmapped | Valid, enabled cartridge RAM only |
| `C000`–`CFFF`                        | Fixed work RAM bank                                                   | Patch fixed bank                  |
| `D000`–`DFFF`                        | Selected work RAM bank                                                | Patch selected bank               |
| `E000`–`FDFF`                        | Echo of work RAM at `C000`–`DDFF`                                     | Patch the corresponding work RAM  |
| `FE00`–`FE9F`                        | Sprite attribute memory, or OAM                                       | Patch OAM                         |
| `FF80`–`FFFE`                        | High RAM, or HRAM                                                     | Patch HRAM                        |
| `FEA0`–`FEFF`, `FF00`–`FF7F`, `FFFF` | Unsupported                                                           | Unsupported                       |

Cartridge access follows the active mapper and its bank selection. MBC2 RAM reads have the upper nibble set to `F`, and
writes retain only the lower nibble. MBC3 real-time-clock register windows raise an error even though they use the
cartridge RAM address range.

Hardware register access, ROM patching, and changing mapper or RAM-bank registers are not exposed through this API.
Unsupported accesses raise errors rather than falling back to CPU bus operations.

## Joypad

Button names are `A`, `B`, `Start`, `Select`, `Up`, `Down`, `Left`, and `Right`.

### `stargbc.joypad.get()` -> table

Returns a new table containing all eight buttons as boolean values. These are the emulator's **physical input states**,
before script overrides. Modifying the returned table does not change input until it is passed to `joypad.set`.

### `stargbc.joypad.set(buttons)`

Replaces the script's input overrides for the upcoming frame. **Only allowed in `before_frame`.** `buttons` must be a
table with supported button names as keys and booleans as values.

- `true` holds that button; `false` releases it even if it is physically held.
- Omitted buttons follow physical input.
- `{}` removes all overrides for the upcoming frame.
- Multiple calls in one callback replace each other; the last table wins.
- The entire table is validated before its overrides are applied.

Supply overrides on every frame that needs them. Holding a button across consecutive frames does not repeatedly release
and press it. Stopping, reloading, failing, or restoring a checkpoint clears script overrides while preserving physical
input. Losing window focus clears both physical input and current overrides; an active script can supply new overrides
on the next frame.

To exclude physical input, specify every button:

```lua
stargbc.on("before_frame", function()
    stargbc.joypad.set({
        A = true, B = false, Start = false, Select = false,
        Up = false, Down = false, Left = false, Right = false,
    })
end)
```

## Snapshots

### `stargbc.state.save()` -> snapshot

Captures the current emulated hardware state and returns an opaque Lua userdata value. Keep it in a variable and pass it
to `state.load`; its contents cannot be inspected or edited through Lua.

Snapshots count toward the Lua memory limit and become eligible for garbage collection when no longer referenced. They
are local to the current script environment and are discarded when it stops or reloads. The API does not read or write
snapshot files.

A snapshot captures the state at the instant of the call. In `before_frame`, the upcoming frame's joypad overrides have
not yet been applied, even if `joypad.set` has already been called.

### `stargbc.state.load(snapshot)`

Requests restoration of a snapshot returned by `state.save`. **Only allowed in `before_frame` or `after_frame`.** It
returns no success value. Restoration is queued until the current callback returns; code later in the same callback
still sees the pre-restore state. The last load request in a callback wins.

| Calling context | Effect after the callback returns                                                                                   |
|-----------------|---------------------------------------------------------------------------------------------------------------------|
| `before_frame`  | Restore, skip the upcoming emulation interval and its `after_frame`, and leave the completed-frame count unchanged. |
| `after_frame`   | Restore after the completed interval; its frame-count increment remains.                                            |

A successful restore clears input overrides and invokes `state_loaded`. A load cannot be requested from `state_loaded`.
Stopping the script or an unhandled error before restoration cancels a queued request.

Snapshots restore emulated hardware, but **do not rewind Lua variables, callbacks, Lua's random generator, the session
frame counter, physical input, or the emulator's pause state**. This lets a script preserve search progress across
attempts.

The following script repeatedly replays 60 frames from an in-memory checkpoint while retaining its attempt counter:

```lua
local checkpoint
local elapsed, attempts = 0, 0

stargbc.on("before_frame", function()
    if checkpoint == nil then
        checkpoint = stargbc.state.save()
    end
end)

stargbc.on("after_frame", function()
    elapsed = elapsed + 1
    if elapsed == 60 then
        elapsed = 0
        attempts = attempts + 1
        stargbc.log(string.format("Completed attempt %d", attempts))
        stargbc.state.load(checkpoint)
        return
    end
end)
```

Restoring the same hardware snapshot and replaying identical input is not a fresh random draw from the emulated game.

## Execution control

### `stargbc.pause()`

Pauses emulation. May be called during initialization or any event callback. The current Lua callback continues until it
returns.

In `before_frame`, pausing prevents the upcoming emulation interval and its `after_frame`. In `after_frame`, the
interval has already completed and remains counted. A queued snapshot restore is still processed even if the same
callback requests a pause.

Pausing does not stop the script or discard its state. Frame callbacks wait until the host resumes emulation; the Lua
API does not provide a resume function.

### `stargbc.stop()`

Disables scripting and releases its input overrides. **Only allowed in event callbacks.** It cancels any queued snapshot
restore and discards the Lua environment after the current callback returns.

Stopping does not pause emulation or automatically return from the callback. Return immediately after calling it. To
preserve an interesting game state for inspection, request both pause and stop:

```lua
stargbc.on("after_frame", function()
    if stargbc.memory.peek8(0xC010) == 0x2A then
        stargbc.log("Target value found")
        stargbc.pause()
        stargbc.stop()
        return
    end
end)
```

## Lua libraries

The base, `table`, `string`, `math`, and `utf8` libraries are available, with these changes:

- `print` uses the script logger.
- `load`, `loadfile`, and `dofile` are unavailable. Scripts must be text, not precompiled bytecode.
- `io`, `os`, `package`, `require`, `debug`, and `coroutine` are unavailable.
- `setmetatable` rejects registration of a non-`nil` `__gc` finalizer. Ordinary metatables and scoped `__close` methods
  are supported.
- `pcall` and `xpcall` can catch ordinary errors, but cannot suppress instruction-budget or memory-limit failures.
- `math.randomseed(0)` is applied to each new environment. A script can explicitly reseed. This generator is separate
  from the emulated game's RNG and is not rewound by `state.load`.

## Limits and error behavior

Limits are set by the host. The defaults are:

| Resource                     | Default limit                                                            |
|------------------------------|--------------------------------------------------------------------------|
| Frame callbacks              | 1,000,000 Lua VM instructions shared by `before_frame` and `after_frame` |
| Initialization               | Ten times the per-frame instruction budget                               |
| Each `state_loaded` callback | One per-frame instruction budget, separate from the frame callbacks      |
| Lua memory                   | 32 MiB, including Lua objects and snapshots                              |
| Script source                | 1 MiB                                                                    |
| One log message              | 4 KiB of message content                                                 |
| Total log output             | 64 KiB of message content per budget period                              |

Instruction usage is checked every 1,000 VM instructions, so very small custom budgets can overshoot by that
granularity. It counts Lua interpreter work rather than emulated CPU instructions and is not a wall-clock deadline for
native library functions. The memory limit covers Lua allocations, not the entire emulator process. A pending restore
also holds one native snapshot copy outside that quota.

Log messages are truncated to the remaining output allowance; further output is dropped when the allowance is exhausted.
The output allowance resets for initialization, each frame, and each `state_loaded` event.

An unhandled error or a resource-limit failure logs an error, disables scripting, releases input overrides, cancels any
pending restore, and destroys the environment. Source locations and tracebacks are included when available. The emulator
remains available; a failed `before_frame` callback leaves physical input in effect for that frame and disables the
script's `after_frame` callback.

Memory patches and other changes already made are not rolled back by an error. Catch an ordinary error with `pcall` if
the script can recover:

```lua
local ok, result = pcall(stargbc.memory.peek8, 0xFF00)
if not ok then
    stargbc.log("Cannot inspect that address: " .. tostring(result))
end
```
