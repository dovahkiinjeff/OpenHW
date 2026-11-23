# Skirmish AI tuning

Difficulty values are 0 Easy, 1 Normal, 2 Hard, 3 Expert. Existing local slider values through 100 preserve their previous difficulty; 101–134 select Expert. Online slots carry the explicit 0–3 value. Expert inherits Hard planning intervals, attack weights, team capabilities and defense features.

The AIPlayer.script loader accepts retail [AI_BEG], [AI_INT], [AI_ADV] and source [AI_BEGINNER], [AI_INTERMEDIATE], [AI_ADVANCED] keys. Expert uses the loaded Hard settings. Normalized keys are scoped to this AI tuning table.

Construction selects the eligible owned factory with the earliest completion time in the requested ship type's lane. Stable ship-list order breaks ties. Temporary reservations cover commands sent into network packets before those packets execute. Factory rosters, loaded ship data and ownership still apply. Campaign/script-directed creators remain explicit.

Carrier expansion target: 1 + floor(cumulative economic output / (3 × carrier price)), subject to ordinary unit caps. Output includes the bank and recorded combat/defense/economic spending. Requests require three harvesters, carrier prerequisites, no pending carrier and a carrier-price-plus-1500-RU bank; essential harvesting and research take precedence. There is no fixed one-carrier or difficulty-specific carrier ceiling.

Expert reserves ceil(active labs / 2) for the combined Heavy Cruiser / Missile Destroyer prerequisite chain. It continues existing prerequisite topics, handles newly built or lost labs, and returns the reserved capacity to normal research after both targets finish. It aims for all six research modules as the economy permits. Remaining labs research normal production needs.

Expert fighter production uses bombers/defenders, corvette production uses heavy/multigun corvettes, and offensive frigate requests favor ion cannon frigates. Specialized support frigates remain available. Research dependencies and normal unit caps are not bypassed.

Expert ship costs and build duration use ceil(normal × 0.9); research duration uses normal × 0.9. These are per-player adjustments; shared ship/technology statics and human/Easy/Normal/Hard costs are unchanged. Research topics currently have zero RU cost, so there is no topic price to reduce. Research ship construction gets the ship price reduction.
