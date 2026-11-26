using CliWrap;

namespace Armonica.Targets;

internal sealed class MonoPOSIXTarget : IBuildTarget {
    private static readonly string[] ZlibSources = [
        "adler32.c", "crc32.c", "deflate.c", "inflate.c", "inffast.c", "inftrees.c", "trees.c", "zutil.c"
    ];

    public string Name => "mono_posix";

    public Task BuildAsync(BuildContext context) =>
        Cli.Wrap(context.Cc)
            .WithArguments([
                "-O2", "-shared",
                "-o", Path.Combine(context.Out, "MonoPosixHelper.dll"),
                $"-I{Path.Combine(context.Native, "mono_posix")}",
                $"-I{context.ZlibDir}",
                "-DHAVE_SYS_ZLIB",
                Path.Combine(context.Deps, "zlib-helper.c"),
                .. ZlibSources.Select(file => Path.Combine(context.ZlibDir, file)),
            ])
            .ExecuteFallible();
}
