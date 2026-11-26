using System.Formats.Tar;
using System.IO.Compression;
using CliWrap;

namespace Armonica;

internal static class Packager {
    internal static void VerifyOutputs(BuildContext context) {
        PackageSpec package = context.Manifest.Package;

        foreach ((string entry, string dll) in package.Files) {
            string path = Path.Combine(context.Out, dll);
            if (!File.Exists(path)) {
                throw new Exception($"missing {dll}");
            }

            (bool isARM64, bool isBuiltin, bool isManaged) = PEImage.Inspect(path);
            if (!isARM64 && !isManaged) {
                throw new Exception($"{dll} isn't arm64");
            }
            if (isBuiltin != entry.StartsWith("wine/", StringComparison.Ordinal)) {
                throw new Exception($"{dll} has wrong wine builtin marker");
            }
        }

        long steamSize = new FileInfo(Path.Combine(context.Out, "steam_api64.dll")).Length;
        if (steamSize >= package.SteamAPIMaxBytes) {
            throw new Exception($"steam_api64.dll is too big ({steamSize} bytes)");
        }
    }

    internal static async Task<string> BuildPackageAsync(BuildContext context) {
        VerifyOutputs(context);

        string commit = (await Cli.Wrap("git").WithArguments(["-c", "safe.directory=*", "-C", context.Root, "rev-parse", "--short=6", "HEAD"]).ExecuteFallible()).StandardOutput.Trim();
        string stage = Path.Combine(context.Dist, $"armonica-{context.Proton.Replace("proton-", string.Empty)}-{commit}");
        string archive = $"{stage}.tar.gz";
        if (Directory.Exists(stage)) {
            Directory.Delete(stage, recursive: true);
        }

        foreach ((string entry, string dll) in context.Manifest.Package.Files) {
            FallibleCommand.CopyFile(Path.Combine(context.Out, dll), Path.Combine(stage, entry));
        }

        FallibleCommand.CopyFile(Path.Combine(context.Root, "LICENSE"), Path.Combine(stage, "LICENSE"));

        CreateArchive(stage, archive);
        Directory.Delete(stage, recursive: true);
        return archive;
    }

    private static void CreateArchive(string sourceDir, string archivePath) {
        string[] files = Directory.GetFiles(sourceDir, "*", SearchOption.AllDirectories)
            .OrderBy(path => Path.GetRelativePath(sourceDir, path).Replace('\\', '/'), StringComparer.Ordinal)
            .ToArray();

        using FileStream fileStream = File.Create(archivePath);
        using var gzip = new GZipStream(fileStream, CompressionLevel.SmallestSize);
        using var tar = new TarWriter(gzip, TarEntryFormat.Ustar);

        foreach (string file in files) {
            string entryName = Path.GetRelativePath(sourceDir, file).Replace('\\', '/');
            using FileStream content = File.OpenRead(file);
            var entry = new UstarTarEntry(TarEntryType.RegularFile, entryName) {
                DataStream = content,
                ModificationTime = DateTimeOffset.UnixEpoch,
                Mode = UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.GroupRead | UnixFileMode.OtherRead
            };
            tar.WriteEntry(entry);
        }
    }
}
