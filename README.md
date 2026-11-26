<h1 align="center">Armonica</h1>

Native Windows ARM64 runtime for running Beat Saber on ARM64 Proton devices

Needs Proton 11.0-3 or newer

## Building

Requires .NET 10 and the native build tools listed in [.github/workflows/release.yml](.github/workflows/release.yml)

```sh
dotnet run --project src/Armonica -- build
dotnet run --project src/Armonica -- package
```

`--proton <tag>` builds against another Proton release

## Releases

The release workflow takes a Proton tag and publishes a signed `armonica-<proton version>-<commit>.tar.gz`

`game/` goes over the install, `mods/` replaces BSIPA's files and `wine/` is the `WINEDLLPATH`
