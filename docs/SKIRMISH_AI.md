# Skirmish AI — 1.1.18

All four difficulties use normal player ship prices, build times and research times. No roster is prohibited. Campaign AI retains its scripted tuning.

| Difficulty | Assault staging timeout | Waiting attack teams | Research capacity | Carrier investment interval | Scouted-counter mission weight |
|---|---:|---:|---:|---:|---:|
| Easy | 480 seconds | 2 | 2 labs | 6 carrier prices | 0 |
| Normal | 300 seconds | 3 | 3 labs | 5 carrier prices | 32 |
| Hard | 180 seconds | 4 | 4 labs | 4 carrier prices | 64 |
| Expert | 120 seconds | 5 | 6 labs | 3 carrier prices | 96 |

These are planning limits, not guaranteed attack periods or forced purchases. Existing difficulty-specific features and reaction settings remain in use. Easy also scales mission-acceptance weights to two thirds. Learned fleet composition biases attack missions without removing specialist roles or changing the ships requested by a mission.

Carrier targets are floor(cumulative economic output / investment interval). Normal unit caps and prerequisites apply. Before an expansion, ordinary attack/defense construction leaves the carrier price plus 500 RU reserved. Urgent defense, scripts and resource-manager purchases can use that money. Queued carriers stop reserving additional expansion funds. Research capacity grows by one module per 4000 RU of recorded economic/attack spending up to the difficulty limit; initial research remains available.

Expert anticipatory research reserves one third of labs only after at least three are present. Observed fighter-heavy fleets favor multigun corvettes and missile destroyers; observed capitals favor ions and heavy cruisers; otherwise it develops destroyers and ions. The remaining labs follow production requests. Ongoing research is never canceled by a changing observation. There are no cost/time discounts.

Explicit online teams are preserved. Free-for-all diplomacy remains enabled when no team is assigned; local allied-victory alliances are preserved. The first proximity sensor can be requested without an existing sensor collection. Resource controllers fall back to their mothership when no remote harvesting cluster exists.

Construction chooses the eligible factory with the earliest completion time in the requested ship lane, including commands awaiting network application. Specialist ship requests are never rewritten. Save structures are unchanged. Older saved games retain their existing initialized planning state; create a new skirmish to evaluate all settings.

No gameplay tests were run for this release at the owner's request. Diagnostic lab binaries, results and deployment configuration are excluded from release packages.
