using CliWrap;

namespace Armonica.Targets;

internal sealed class OpenXRLoaderTarget : IBuildTarget {
    public string Name => "openxr_loader";

    public Task BuildAsync(BuildContext context) =>
        Cli.Wrap(context.Cxx)
            .WithArguments([
                "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                "-ffunction-sections", "-fdata-sections",
                "-Wall", "-Wextra",
                $"-I{context.Native}",
                "-shared", "-s", "-static", "-Wl,--gc-sections",
                "-o", Path.Combine(context.Out, "openxr_loader.dll"),
                Path.Combine(context.Native, "openxr_loader", "loader.cpp"),
            ])
            .ExecuteFallible();
}
