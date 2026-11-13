# OpenHW 1.1 - fresh installation and dedicated multiplayer preview

**1.1 is a complete fresh-install release. No earlier OpenHW installer or patch
chain is required.** Put OpenHW-Setup.exe in a new writable game folder, run it
and select your owned Homeworld Remastered Collection. It installs beside setup
and imports needed Classic/Remastered data locally without changing the
Collection. Old OpenHW installations are not valid destinations; back up saves
and custom work separately.

Setup includes **OpenHW.exe, OpenHW-Daignostics.exe, Launcher.exe and Updater.exe**.
Both game executables are Windows x64. Logging is compiled out of the normal
game; full diagnostics write under Development. Settings, saves, mods,
downloads and application-owned caches stay in the portable folder. Display
size/refresh detect on first boot.

## Server-only timing hotfix - November 13, 2026

Restores normal multiplayer simulation speed. The gateway now supplies 32
command frames per second, matching two physics steps per frame at the modern
64-tick physics rate. The earlier 8 Hz cadence ran matches at quarter speed and
left long gaps between movement updates. Ship stats and client EXEs are unchanged.
Start a new match using your existing 1.1 installation; no new game download is
needed. Live testing measured 4.969 seconds of simulation in 4.969 real seconds,
with both replicas synchronized. The cumulative source includes this server fix.

## Dedicated multiplayer preview

Online Multiplayer uses the OpenHW server to run simulation/AI, validate
ownership and distribute canonical battle frames. Create a room and share its
six-character code. Players need no port forwarding. The standard 1.1
executables include this preview; separate preview EXEs are unnecessary.

Hosts can add AI, choose race/difficulty, assign numbered teams or free-for-all,
select Allied Victory and configure match gameplay rules. Fuel, resource
population/yield, ranks, starting fleets, caps, fog of war and capture/build
options apply consistently. Graphics/audio/camera/cursor stay personal.
Changes clear readiness; rules lock at start. Only the host can pause a battle.

RTS orders, immediate formation spread, direct piloting, EVE-style Command,
eligible X/Claw wingleader control and individual appearance use the server
command stream. Existing eligibility and control keys still apply.

This remains a **multiplayer preview**. Current capacity is one running battle,
with map limits governing up to eight human/AI slots. Private invite rooms are
included; accounts, a public browser and matchmaking are not. A human disconnect
ends the battle. Running-battle rejoin, online saves/resume and battle host
migration are not implemented. Build/world mismatch or detected divergence ends
the match. Arbitrary gameplay-mod synchronization is unsupported. Use matching
unmodified 1.1 installs. See [the multiplayer guide](MULTIPLAYER_PREVIEW.md).

## Community corrections

- **Tutorial:** repairs Next/Back button storage/input, defers restart until its
  active callback returns and advances cached-image cleanup.
- **Sensors/managers:** Build/Research/Launch preserve Sensors and return to it
  on close. F focuses selected ships and exits Sensors. Closing Sensors retires
  its open managers.
- **HUD:** Backspace cycles full, compact Build/Research/Launch and hidden modes
  outside scripted tutorial control. Hidden controls stop receiving clicks.
  Cinematics suppress ordinary/compact HUD; cinematic bars remain removed.
- **Capture delivery:** restores a coherent Classic mothership entry route and
  outer arrival envelope, addressing captured-ship delivery near the mothership.
- **Khar-Selim:** rechecks native wreck contact on final approach and aligns the
  corvette grip before latching. Saved clamp offsets are repaired while
  preserving mission salvage state.
- **Starting Resource Fleet:** additions wait for hyperspace arrival and start
  at parade berths. Existing orders/larger fleets remain; saves inject no ships.
- **Research/audio:** restores docked research description/unlock details and
  Battle Chatter Frequency on the Voice tab. Voice Chatter remains separate;
  saved defaults remain.
- **Navigation/scout engines:** native hull lamp markers extend beyond Support
  Frigates, retaining Classic fallback and authored overrides. Kushan/Taiidan
  Scout extra bulb/trail spacing is halved without moving authored nozzles or
  changing bulb appearance.

## Diagnostics and installation

The main-menu Diagnostics action opens a report window through the launcher.
Review selected logs, enter an alias/comment and optionally attach a save
before choosing to upload. Reports use private server storage. Sending is
explicit; normal play does not automatically upload files.

Setup rejects old installations even without their completion receipt.
Interrupted 1.1 conversion resumes only with its matching recorded payload.
The compact importer selects required HW1 content, cleans completed temporary
stages and consumes final packing inputs to reduce duplicate disk use.
The main archive is approximately 33.61 GB; installation/conversion need
additional space. Allow approximately 80 GB free on NTFS with hard links, or
100 GB when hard links are unavailable; these are conservative estimates and
the exact full peak remains unmeasured. See
[the user guide](USER_GUIDE.md).

Full cumulative source includes engine/shaders, importer, launcher/updater,
dedicated server, tests/notices, reviewed profile and documentation images.
Owned retail data and private server files are excluded. Enhanced video
cutscenes remain a separate optional download.

## Graphics, authoring and verification

**AMD FSR upscaling and FSR frame generation remain recommended.** DLSS has known
visual issues; Intel upscaling is not recommended. NVIDIA/Intel frame generation
are not implemented. Path tracing needs compatible DXR hardware. No universal
FPS gain is promised.

Public 1.1 executables disable live **Shift+F11/F12 map/lighting editors**.
Authored maps, skies, lighting and clouds still load. Source builds can enable
the developer authoring option. Ship/parade editing retains its availability.
For cumulative features and images, see the [feature tour](FEATURE_TOUR_1.0.7.md)
and [user guide](USER_GUIDE.md).

Offline production-code tests cover tutorial, HUD/input, capture/salvage,
loading and packaging. Live server checks compare normal/diagnostic replicas,
AI/teams, ownership validation, pilot/EVE/wing commands, appearance and host
pause. They do not replace player verification of full battles, tutorial
progression, busy hangars or model alignment. Report reproducible problems
with mission/settings/diagnostics; do not upload retail game data.
