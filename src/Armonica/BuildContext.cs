using System.Runtime.InteropServices;
using Microsoft.Extensions.Logging;

namespace Armonica;

internal sealed class BuildContext {
    internal string Root { get; }
    internal string Deps { get; }
    internal string Proton { get; }
    internal string Generated { get; }
    internal string Obj { get; }
    internal string Out { get; }
    internal string Dist { get; }
    internal string Native { get; }
    internal int Jobs { get; }
    internal string HostArch { get; }
    internal Manifest Manifest { get; }
    internal ILoggerFactory LoggerFactory { get; }

    internal string LLVMMingw => Path.Combine(Deps, "llvm-mingw");
    internal string Cc => Path.Combine(LLVMMingw, "bin", "aarch64-w64-mingw32-clang");
    internal string Cxx => Path.Combine(LLVMMingw, "bin", "aarch64-w64-mingw32-clang++");
    internal string Dlltool => Path.Combine(LLVMMingw, "bin", "aarch64-w64-mingw32-dlltool");
    internal string ProtonDir => Path.Combine(Deps, Proton);
    internal string WineDir => Path.Combine(ProtonDir, "wine");
    internal string WineToolsDir => Path.Combine(Deps, $"{Proton}-wine-tools");
    internal string ZlibDir => Path.Combine(Deps, $"zlib-{Manifest.Components.ZlibVersion}");
    internal string BSIPADir => Path.Combine(Deps, "bsipa");
    internal string MonoModDir => Path.Combine(Deps, $"monomod-core-{Manifest.Components.MonoModVersion}");

    private BuildContext(string root, string deps, string proton, Manifest manifest, ILoggerFactory loggerFactory) {
        Root = root;
        Deps = deps;
        Proton = proton;
        Generated = Path.Combine(root, "generated");
        Obj = Path.Combine(root, "obj");
        Out = Path.Combine(root, "out");
        Dist = Path.Combine(root, "dist");
        Native = Path.Combine(root, "native");
        Jobs = Math.Max(1, Environment.ProcessorCount);
        HostArch = RuntimeInformation.OSArchitecture switch {
            Architecture.Arm64 => "aarch64",
            Architecture.X64 => "x86_64",
            var other => throw new Exception($"unsupported host arch: {other}")
        };
        Manifest = manifest;
        LoggerFactory = loggerFactory;

        Directory.CreateDirectory(Deps);
        Directory.CreateDirectory(Obj);
        Directory.CreateDirectory(Out);
    }

    internal static BuildContext Create(string root, string? depsOverride, string? protonOverride, ILoggerFactory loggerFactory) {
        Manifest manifest = Manifest.Load(Path.Combine(root, "manifest.json"));
        string deps = depsOverride
            ?? Environment.GetEnvironmentVariable("ARMONICA_DEPS_DIR")
            ?? Path.Combine(root, "deps");
        return new BuildContext(root, Path.GetFullPath(deps), protonOverride ?? manifest.Proton, manifest, loggerFactory);
    }

    internal Dictionary<string, string?> ToolchainEnv() => new() {
        ["PATH"] = $"{Path.Combine(LLVMMingw, "bin")}{Path.PathSeparator}{Environment.GetEnvironmentVariable("PATH") ?? string.Empty}"
    };
}
