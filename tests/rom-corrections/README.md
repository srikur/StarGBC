# GBMicrotest assertion review

The original ROMs are intentionally retained. Both remain registered and fail
normally; there are no expected-failure markers or expectation overrides.
The two changes in [gbmicrotest.patch](gbmicrotest.patch) document a possible
upstream correction against
[the audited source](https://github.com/aappleby/gbmicrotest/tree/463eb6bc0fe31d61781ef63060ad6d74090c0255).
They have **not** been applied to the bundled ROMs or automated expectations.

| ROM | Bundled assertion | Observed value | Proposed assertion |
| --- | --- | --- | --- |
| `halt_op_dupe_delay` | DIV = `55` | `01` | DIV = `01` |
| `stat_write_glitch_l154_d` | IF = `E0` | `E1` | IF = `E1` |

StarGBC and independently built SameBoy 1.0.3 produce the same observed values
using the bundled DMG boot ROM.

`halt_op_dupe_delay` clears DIV, executes HALT with IME disabled and STAT already
pending, then runs a NOP, 58 more NOPs, and reads DIV. This spans roughly one
256-clock DIV increment. `55` would require 21,760 clocks. The finish macro
reads DIV directly; `55` is not a success sentinel at this point.

`stat_write_glitch_l154_d` disables interrupts, clears IF before enabling the
LCD, then waits 154 scanlines plus 110 NOPs. VBlank has set IF bit 0 during that
wait. The program never clears IF again or services VBlank, so the final STAT
write cannot make IF read `E0`. The value `E1` retains that pending VBlank bit.

The proposed changes preserve the tests' instruction timing and checks. The
source patch passes `git apply --check` against the audited upstream tree.
Temporary copies with the changed comparison and result constants both produce
FF82=`01` in StarGBC. The repository keeps the original ROMs and reports these
two failures separately instead of applying those corrections.
