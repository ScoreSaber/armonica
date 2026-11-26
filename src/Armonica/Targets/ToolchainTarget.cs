using CliWrap;

namespace Armonica.Targets;

internal sealed class ToolchainTarget : IBuildTarget {
    public string Name => "toolchain";

    public async Task BuildAsync(BuildContext context) {
        if (File.Exists(context.Cc)) {
            return;
        }

        string version = context.Manifest.LLVMMingwVersion;
        string name = $"llvm-mingw-{version}-ucrt-ubuntu-22.04-{context.HostArch}";
        string archive = Path.Combine(context.Deps, $"{name}.tar.xz");
        await FallibleCommand.DownloadAsync($"https://github.com/mstorsjo/llvm-mingw/releases/download/{version}/{name}.tar.xz", archive);
        await Cli.Wrap("tar").WithArguments(["xJf", archive, "-C", context.Deps]).ExecuteFallible();
        File.Delete(archive);
        if (Directory.Exists(context.LLVMMingw)) {
            Directory.Delete(context.LLVMMingw, recursive: true);
        }
        Directory.Move(Path.Combine(context.Deps, name), context.LLVMMingw);
    }
}
