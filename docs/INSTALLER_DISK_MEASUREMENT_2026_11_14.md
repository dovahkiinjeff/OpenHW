# Full installer disk measurements - 2026-11-14

Two complete, clean installations ran sequentially on a fixed NTFS N: volume. Both used the same owned Steam Homeworld Remastered Collection as a read-only source. The baseline was the exact public **1.1.1 base installer**, not the older retired broad-extraction installer. The corrected candidate replaced only three embedded converter modules; every other payload file and both game executables were unchanged.

| Measurement | Public 1.1.1 baseline | Corrected candidate |
|---|---:|---:|
| Observed peak additional drive allocation | 39.304 GB / 36.605 GiB | 37.913 GB / 35.309 GiB |
| Peak logical directory size | 39.969 GB / 37.224 GiB | 37.789 GB / 35.193 GiB |
| Final additional drive allocation | 36.922 GB / 34.387 GiB | 35.876 GB / 33.412 GiB |
| Final main OBIG archive | 33.611 GB | 32.597 GB |

The measured peak reduction was **1,391,161,344 bytes (1.391 GB, 3.5%)**. Logical directory size fell by 2.181 GB. The archive shrank by 1.014 GB. No textures were resized or recompressed and no model quality was reduced. The earlier 80-100 GB figures were unmeasured planning allowances; they are not measurements of the current installer.

## Copies removed

- Repeated extraction of an immutable model/image now reuses an already imported file only after verifying identical bytes. Retail archives themselves are never linked or modified. Editable text is copied independently; filesystems without hard links retain the safe copy fallback.
- Model-only source scratch is retired after its final FX consumer. Event/script sources and subsystem models required by later stages are preserved.
- The temporary model archive used by document migration is removed before final packing. A digest identifies the expected placeholder; a changed file is preserved. Durable prepared/journal state distinguishes a completed final archive during recovery.
- Backdrop consumers use SDL loose-file reads. Every published loose backdrop is verified before its redundant OBIG input is removed. The loose data and all rendering inputs remain present.

## Validation

Both actual frozen installers completed the entire conversion, native asset validation, final packing, all mission lighting skies and success cleanup. All **23,038 retained archive resources** have identical names, sizes and CRCs. All **1,655 other installed loose files** have identical SHA-256 hashes. Sixteen translation audit JSON reports differ only in their recorded installation-root source path.

The only archive omissions are **112 redundant backdrop entries** (1,013,768,123 payload bytes). Their loose counterparts remain installed: 96 are SHA-256-identical and the 16 audit reports have only the expected source-path difference. No runtime resources are missing. Both installs remove SetupSources and SetupWork on success. Their source Collection archive hashes, sizes and modification-time receipts match.

Seventeen focused installer disk/recovery tests and eight general installer tests pass. They cover verified reuse, editable-text independence, no-hard-link fallback, source cleanup, changed-file preservation, consuming-pack recovery and removal of redundant backdrop inputs.

## Measurement method and limits

Windows free space on the destination volume was sampled every **250 ms**, from copying the installer through bootstrap extraction, payload verification, full conversion, packing, final sky generation and cleanup. Directory snapshots every **15 seconds** also record logical sizes and allocation by distinct Windows file ID, so hard links count once. The volume delta includes filesystem metadata and the small measurement logs; no other large task wrote to N: during either run. The observed peak can exceed a directory scan because short-lived files can fall between scans.

GB means 1,000,000,000 bytes; GiB means 1,073,741,824 bytes. These are observed peaks for the tested Collection, payload and NTFS volume, not guaranteed maximum requirements on every filesystem. The original Collection, future patches, user mods, saves and later game caches are outside this fresh-install measurement. No complete copy-fallback filesystem run was performed. Timing is not a controlled performance benchmark because the second run can benefit from cached source reads.

The baseline's first directory counter used Windows DirEntry inode values, which are zero. A corrected independent observer was attached during initial import and corrected the final allocation report. The complete 250 ms volume measurement and logical directory measurements were unaffected. The reusable meter uses os.stat to obtain actual Windows file IDs.

Reproduce with an unused destination folder, on an otherwise quiet volume:

```powershell
python tools/measure_installer_disk.py N:/OpenHW-Audit/run-name --installer PATH/TO/OpenHW-Setup.exe --collection PATH/TO/Homeworld
```

The meter invokes the quiet bootstrap verification and then the actual frozen installer with --collection. It never launches the game. Its reports and installation stay inside the chosen run folder. The fresh destination must be separate from the Collection. Keep headroom above observed peaks rather than treating a sampled peak as a guaranteed minimum.

Machine-readable results are in [the measurement record](INSTALLER_DISK_MEASUREMENT_2026_11_14.json). The corrected candidate is validated locally; the public download has not been replaced by this audit.
