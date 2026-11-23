# OpenHW 1.1.16 — AI production, Expert difficulty and update handoff

Choose **Update now** in Launcher. OpenHW 1.1.15 installations receive a patch in place; saves, settings, badges and mods are preserved. Compiled updates are hosted on openhw.tech; GitHub contains source and release notes.

- Fixed AI configuration loading: retail AI_BEG / AI_INT / AI_ADV keys now reach the actual difficulty settings.
- AI distributes construction across its eligible mothership and carriers, choosing the shortest construction lane. It respects each factory's build roster and avoids destroyed, disabled and hyperspacing factories.
- Removed the one-carrier purchasing ceiling. AI expands its production as economic output increases, while retaining money for combat and respecting unit caps.
- Research-blocked requests no longer prevent already-researched ships later in the queue from being built.
- Added **Expert** to local skirmish/LAN and the online lobby. It inherits configured Hard behaviors, reserves half of active research labs for Heavy Cruiser and Missile Destroyer prerequisites, and releases that reservation once both are researched. Odd lab counts round the reserved half up.
- Expert builds bombers and defenders as fighters, heavy and multigun corvettes as corvettes, and prioritizes ion cannon frigates for its offensive frigate production.
- Expert ship prices and ship/research durations are 90% of normal. Research topics have no RU fee in this engine, so they remain free. Research ships receive the ship discount.
- Fixed an incomplete recursive research dependency traversal so advanced technology prerequisites are included.
- Launcher/Updater utility version **1.1.10** fixes a full-install handoff race: the Launcher stays closed while the installer runs. Setup also waits for the destination folder's Launcher to finish closing and ignores Launchers in unrelated installations.
- Updated the authoritative multiplayer worker and lobby validation for Expert.

