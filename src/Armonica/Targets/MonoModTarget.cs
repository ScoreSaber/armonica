using System.IO.Compression;

namespace Armonica.Targets;

internal sealed class MonoModTarget : IBuildTarget {
    public string Name => "monomod";

    public async Task BuildAsync(BuildContext context) {
        string version = context.Manifest.Components.MonoModVersion;
        Directory.CreateDirectory(context.MonoModDir);
        string pkg = Path.Combine(context.MonoModDir, $"monomod.core.{version}.nupkg");
        if (!File.Exists(pkg)) {
            await FallibleCommand.DownloadAsync($"https://api.nuget.org/v3-flatcontainer/monomod.core/{version}/monomod.core.{version}.nupkg", pkg);
        }

        string actual = FallibleCommand.SHA256File(pkg);
        if (!string.Equals(actual, context.Manifest.Components.MonoModSHA256, StringComparison.OrdinalIgnoreCase)) {
            throw new Exception($"monomod checksum mismatch (got {actual})");
        }

        using ZipArchive archive = ZipFile.OpenRead(pkg);
        ExtractEntry(archive, "lib/net452/MonoMod.Core.dll", Path.Combine(context.Out, "MonoMod.Core.dll"));
    }

    private static void ExtractEntry(ZipArchive archive, string entryPath, string destination) {
        ZipArchiveEntry entry = archive.GetEntry(entryPath) ?? throw new Exception($"missing {entryPath} in nupkg");
        using Stream input = entry.Open();
        using FileStream output = File.Create(destination);
        input.CopyTo(output);
    }
}
