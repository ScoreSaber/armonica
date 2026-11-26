using System.Formats.Tar;
using System.IO.Compression;
using CliWrap;

namespace Armonica.Targets;

internal sealed class FetchTarget : IBuildTarget {
    public string Name => "fetch";

    public async Task BuildAsync(BuildContext context) {
        ComponentsSpec components = context.Manifest.Components;

        if (!Directory.Exists(Path.Combine(context.ProtonDir, ".git"))) {
            await Cli.Wrap("git")
                .WithArguments(["clone", "-q", "--depth", "1", "--branch", context.Proton, "https://github.com/ValveSoftware/Proton.git", context.ProtonDir])
                .ExecuteFallible();
        }

        string wineCommit = (await Cli.Wrap("git").WithArguments(["-C", context.ProtonDir, "rev-parse", "HEAD:wine"]).ExecuteFallible()).StandardOutput.Trim();
        await FallibleCommand.GitFetchCommitAsync("https://github.com/ValveSoftware/wine.git", wineCommit, context.WineDir);

        if (!Directory.Exists(context.ZlibDir)) {
            string archive = Path.Combine(context.Deps, $"zlib-{components.ZlibVersion}.tar.gz");
            await FallibleCommand.DownloadAsync($"https://github.com/madler/zlib/releases/download/v{components.ZlibVersion}/zlib-{components.ZlibVersion}.tar.gz", archive);
            await using (FileStream fs = File.OpenRead(archive))
            await using (var gz = new GZipStream(fs, CompressionMode.Decompress)) {
                TarFile.ExtractToDirectory(gz, context.Deps, overwriteFiles: true);
            }
            File.Delete(archive);
        }

        await FallibleCommand.GitFetchCommitAsync("https://github.com/nike4613/BeatSaber-IPA-Reloaded.git", components.BSIPACommit, context.BSIPADir);

        string zlibHelper = Path.Combine(context.Deps, "zlib-helper.c");
        if (!File.Exists(zlibHelper)) {
            await FallibleCommand.DownloadAsync($"https://raw.githubusercontent.com/Unity-Technologies/mono/{components.UnityMonoCommit}/support/zlib-helper.c", zlibHelper);
        }
    }
}
