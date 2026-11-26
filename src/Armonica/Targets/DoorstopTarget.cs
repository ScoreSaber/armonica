using Armonica.Codegen;
using CliWrap;

namespace Armonica.Targets;

internal sealed class DoorstopTarget : IBuildTarget {
    public string Name => "doorstop";

    public Task BuildAsync(BuildContext context) {
        string nativeDir = Path.Combine(context.Native, "doorstop");
        string proxySrc = Path.Combine(context.BSIPADir, "Doorstop", "Proxy");
        string generated = Path.Combine(context.Generated, "doorstop", "proxy_arm64.c");

        DoorstopCodegen.Generate(Path.Combine(proxySrc, "proxy.c"), generated);

        return Cli.Wrap(context.Cc)
            .WithArguments([
                "-O2", "-fgnu89-inline", "-fno-builtin", "-shared", "-nostdlib",
                "-Wl,--entry,DllMain",
                "-DUNICODE", "-D_UNICODE", "-DNDEBUG", "-D_WINDOWS", "-D_USRDLL", "-DNOGDI",
                $"-I{Path.Combine(nativeDir, "include")}",
                $"-I{proxySrc}",
                "-o", Path.Combine(context.Out, "winhttp.dll"),
                Path.Combine(nativeDir, "entry.c"),
                Path.Combine(nativeDir, "crt_compat.c"),
                generated,
                Path.Combine(proxySrc, "proxy.def"),
                "-lkernel32", "-luser32", "-lshell32", "-ladvapi32", "-lshlwapi", "-lucrt",
            ])
            .ExecuteFallible();
    }
}
