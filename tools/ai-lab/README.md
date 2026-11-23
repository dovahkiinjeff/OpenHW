# OpenHW AI laboratory (source only)

Separate administrator tooling for studying skirmish AI. **Do not package this
directory, its instrumented worker, configurations or logs in a game release.**
The production executable, public gateway and update feed are not modified.
There is no HTTP listener or player-accessible administration endpoint.

## Experiments

The default batch runs ten concurrent, genuine eight-AI matches: two teams of
four on Hard. It randomizes Kushan/Taiidan mixes, team slot assignments, map and
RNG seed. Only installed eight-player maps are eligible. Normal research,
harvesting, fuel, caps, fog and ship rules remain in force; allied victory and
hyperspace are enabled. Costs and simulation timestep are unchanged.

Acceleration removes wall-clock sleeps. Every canonical command frame still
advances the same two 1/64-second physics ticks as multiplayer. Ten games share
2.5 CPU cores and a 10 GiB memory ceiling by default, leaving resources for the
public service. Two workers initialize at once to reduce loading spikes.

Defaults: two simulated hours per match, six real hours per batch, snapshots
every ten simulated seconds. A match can finish earlier through ordinary
victory. Long runs do not restart themselves.

## Build separately

Run `prepare_engine.py --source /path/to/reviewed/source --destination
/path/to/new/lab-source`. It refuses an existing destination. Configure and
build `homeworld` in that new source tree using the repository's normal Windows
dependencies, with Release optimization and diagnostics enabled. The private
copy adds an eight-AI entry point, decision traces and read-only telemetry.
It never patches the original source or installs its worker into the game.
Name the resulting private executable `OpenHW-AI-Lab.exe`.

On a Linux host with Wine, install the lab source together with
`server/protocol.py`, then place the private worker and its DLL dependencies
outside the public game directory. Use a dedicated `openhw-ai-lab` OS account,
a private Wine prefix, and read-only access to imported game assets. The CLI's
systemd sandbox makes the filesystem read-only except for its results, Wine
prefix and dedicated home. No per-match copies of the retail archive are needed.
Native headless worlds still load substantial model data: provision enough
RAM/swap for roughly 1.5–2 GiB per worker plus the host. The lab permits up to
8 GiB of swap in addition to its 10 GiB resident limit; it does not create swap
or change host memory settings automatically. Cold model pages can be paged out
without changing AI decisions or simulation ticks.

Copy `config.example.json` to a private location such as
`/etc/openhw-ai-lab/config.json`. Fill in paths and the installed map capacities.
Do not commit the filled configuration. No server address, login, token, private
key or password belongs in this source. SSH aliases and authentication remain
in the operator's existing, private SSH configuration.

## Commands

From the server (administrator):

```text
python3 tools/ai-lab/control.py start --games 10 --difficulty 2 --seed 20261123
python3 tools/ai-lab/control.py status
python3 tools/ai-lab/control.py report
python3 tools/ai-lab/control.py stop
```

From an operator's computer, add `--ssh YOUR_PRIVATE_SSH_ALIAS` before the
command. Supply `--remote-source` or `--config` when using other installation
paths. This supports commands issued by the user or an assistant using their
authorized SSH session. **Tell the user immediately before starting real
matches.** Merely installing, compiling or running unit tests does not start any.

Difficulty values: 0 Easy, 1 Normal, 2 Hard, 3 Expert. Reuse a batch seed to
compare difficulties or candidate changes on the same race mixes, slots, maps
and game seeds. Do not mix engine revisions within one batch.

## Recorded evidence and storage

Each run retains its exact setup, worker hash and world signatures; complete
canonical command frames; AI decision messages and crash output; sampled player
economy/research, factory build orders, team membership/current order, ship
health/position/orders/attack targets, and world resource totals. Rendering and
audio diagnostics are irrelevant to this headless test.

The byte budget covers **all retained diagnostic runs together**, not 100 GB
per game. It is clamped to 100,000,000,000 bytes even if configuration requests
more. Existing files count before a run starts. One event-loop writer reserves
space before each write; a small reserved allowance permits final status and
error reports. Reaching the limit stops the entire batch, as do operator stop,
low disk space or the wall-time deadline. Engine logs rotate at 64 MiB. Nothing
automatically deletes previous evidence. Wine/runtime dependencies are separate
from the diagnostic collection and are not duplicated per match.

`analysis.json` flags stationary ships, unchanged team orders, invalid positions
and stagnant economies. These are observations, not proof of a bug: a guarding
ship can legitimately remain stationary. Inspect the surrounding decision trace,
research/build queues and target state before proposing an AI change. Keep
reports tied to the engine hash, difficulty and seeds. Logs guide explicit code
and tuning changes; they are not automatically fed back into gameplay.

Unit tests: `python -m unittest discover -s tools/ai-lab -p test_lab.py`.
