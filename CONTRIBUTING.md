# Contributing

Homeworld RTX welcomes focused, testable improvements that preserve the classic
simulation while advancing the Windows renderer and authoring tools.

## Before opening a change

- Read `LICENSE.txt`; contributions must be compatible with the restrictive
  non-commercial Relic source baseline.
- Never commit retail BIG archives, audio, movies, extracted game data, secrets,
  crash dumps, dependency caches, or compiled NVIDIA SDK contents.
- Keep presentation changes out of the fixed simulation clock unless the change
  explicitly targets gameplay and documents save/multiplayer impact.
- Prefer non-destructive sidecars and editor exports for content changes.

## Build and test

```powershell
./Windows/bootstrap.ps1
./Windows/build.ps1 -Configuration Release
```

For renderer work, test a representative campaign mission, a dense battle,
window resize/Alt+Tab, save/load, UI managers, and both editors. State your GPU,
driver, Windows version, and data edition in the pull request.

## Code and documentation

- Follow the repository `.clang-format` where it applies.
- Keep C and C++ changes scoped and preserve legacy behavior intentionally.
- HLSL changes must compile as Shader Model 6.3 and include a fallback policy.
- Update `documentation/GRAPHICS_FEATURE_STATUS.md` when implementation status changes.
- Update user/editor documentation for every visible control, range, shortcut,
  file format, or compatibility boundary.
- Distinguish measured behavior from planned roadmap work.
- Save documentation as UTF-8. Publish GitHub release descriptions directly from
  the UTF-8 file with `gh release edit <tag> --notes-file <path>` (or
  `gh release create <tag> --notes-file <path>` for a new release). Do not pass
  notes through a Windows PowerShell `Get-Content` call with its default encoding;
  it can turn valid dashes, arrows and accented characters into unreadable text.

## Reports

Use a minimal reproduction and attach project-generated logs only. Do not attach
retail game archives or assets. Security-sensitive reports should follow
`SECURITY.md` instead of being filed publicly.
