# Community report corrections - OpenHW 1.1

These changes are included in OpenHW 1.1. Player verification of full mission and battle behavior remains important.

- **Capture delivery:** restored the coherent Classic salvage entry route and outer arrival envelope. The 1.0.7 close holding point combined a Remastered interior waypoint with Classic approach directions and could prevent a towed frigate from reaching the port-reservation stage. This concerns delivery into the mothership, rather than initial capture success.
- **Khar-Selim:** native wreck contacts are rechecked at final approach. Both native salvage corvettes now account for their own forward grip when aligning with the wreck surface. Heading must align before latching. Saved relative clamp offsets are repaired without changing the mission's salvage state.
- **Starting Resource Fleet:** additions wait for campaign hyperspace arrival and start at their parade berths. Only newly added ships are positioned; existing player orders and larger carried fleets are preserved. Loading a save does not inject new ships.
- **Research:** restored a dedicated description/unlock panel in the docked manager. Installed research descriptions already existed; hiding the legacy image region had also hidden the callback drawing those descriptions.
- **Audio:** restored the existing Battle Chatter Frequency control to the modern Voice tab. The Voice Chatter enable switch remains separate. Saved defaults are unchanged.
- **Resource settings:** clarified adaptive population versus harvested RU yield. No balance override or automatic change to player preferences was applied based on the report.
- **Tutorial:** corrected undersized Next/Back button allocations and their click processing. Lesson restart is deferred until the active region callback has returned; cached-image cleanup now advances through its list.
- **Sensors/managers:** docked managers preserve the Sensors screen and return to it on close. F focuses selected ships and exits Sensors. Closing Sensors also retires its open managers.
- **HUD:** Backspace cycles full, compact Build/Research/Launch, and hidden states outside the scripted tutorial. Hidden controls detach from input. Cinematic scenes suppress the ordinary HUD/compact strip. Cinematic black bars remain removed, following the project owner's earlier request.
- **Navigation lights:** exact native lamp marker positioning now applies to native ship hulls beyond the Support Frigate. Missing markers retain Classic fallback; explicit authoring overrides retain priority. The installed Kushan collector's four lamp coordinates were checked through the native reader.
- **Scout glow:** halves only the extra bulb/trail spacing for the native Kushan and Taiidan Scouts. Authored nozzle positions, bulb appearance and Classic fallback are retained. Bulb and ribbon history share the same anchor.

Validation uses native model readers, actual production C routines and headless checks. No automated game-window or camera interaction is performed. A played tutorial, busy capture delivery and the visible presentation still need player verification before public release.
