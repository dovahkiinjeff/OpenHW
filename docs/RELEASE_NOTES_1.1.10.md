# OpenHW 1.1.10 - Measured installer disk improvements

**Existing players:** choose **Update now** in Launcher. The small maintenance patch updates installer/conversion support and the recorded version. It preserves the game executables, saves, settings, badges, mods and authored maps. **No reinstall is needed.** Multiplayer code is unchanged, so no server restart is required.

**New players:** the fresh-install download now uses the exact lower-space installer validated in two full NTFS installation runs. It installs the supported **1.1.1 base**, then Launcher applies the current patches through 1.1.10. Put the full installer in a new empty folder; never use it over an existing game.

| Complete installation measurement | Previous public installer | Refreshed installer |
|---|---:|---:|
| Observed peak additional drive use | 39.30 GB | 37.91 GB |
| Peak logical directory size | 39.97 GB | 37.79 GB |
| Final main game archive | 33.61 GB | 32.60 GB |

GB is decimal. These are observed full-install results on NTFS, not universal maximum space requirements. The older 80-100 GB figure was an unmeasured allowance.

The converter reuses byte-verified immutable model/image imports, retires model scratch after its last consumer, removes the temporary migration archive before final packing, and omits a redundant packed copy of already verified loose backdrops. No textures were resized or recompressed.

Both complete installations passed native asset validation. All 23,038 retained archive resources matched names, sizes and CRCs, and 1,655 other loose files matched SHA-256. Sixteen translation audit reports differed only in their installation-root path. All 112 omitted archive entries remain installed as verified loose backdrop files. Seventeen disk/recovery tests and eight installer tests passed. See the full measured report in the documentation.

Compiled installer and patch downloads are hosted only on openhw.tech. GitHub hosts source, source checksums and these release notes. Existing supported installations continue to receive patches in order.
