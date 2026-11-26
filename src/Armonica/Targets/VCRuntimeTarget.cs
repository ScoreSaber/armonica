using CliWrap;

namespace Armonica.Targets;

internal sealed class VCRuntimeTarget : IBuildTarget {
    public string Name => "vcruntime";

    public Task BuildAsync(BuildContext context) {
        string dir = Path.Combine(context.Native, "vcruntime");
        return Cli.Wrap(context.Cxx)
            .WithArguments([
                "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                "-ffunction-sections", "-fdata-sections",
                "-Wall", "-Wextra",
                "-shared", "-s", "-static", "-Wl,--gc-sections",
                "-o", Path.Combine(context.Out, "vcruntime140.dll"),
                Path.Combine(dir, "vcruntime.cpp"),
                Path.Combine(dir, "vcruntime140.def"),
            ])
            .ExecuteFallible();
    }
}
