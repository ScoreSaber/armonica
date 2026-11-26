using CliWrap;

namespace Armonica.Targets;

internal sealed class LIVBridgeTarget : IBuildTarget {
    public string Name => "liv_bridge";

    public Task BuildAsync(BuildContext context) =>
        Cli.Wrap(context.Cc)
            .WithArguments(["-shared", "-Os", "-s", "-o", Path.Combine(context.Out, "LIV_Bridge.dll"), Path.Combine(context.Native, "liv_bridge", "stub.c")])
            .ExecuteFallible();
}
