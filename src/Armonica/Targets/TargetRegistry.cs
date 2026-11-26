namespace Armonica.Targets;

internal static class TargetRegistry {
    internal static IReadOnlyList<IBuildTarget> All { get; } = [
        new ToolchainTarget(),
        new FetchTarget(),
        new WineToolsTarget(),
        new ProtonModuleTarget("lsteamclient"),
        new ProtonModuleTarget("wineopenxr"),
        new SteamAPITarget(),
        new OpenXRLoaderTarget(),
        new VCRuntimeTarget(),
        new MonoPOSIXTarget(),
        new DoorstopTarget(),
        new MonoModTarget(),
        new LIVBridgeTarget(),
    ];

    internal static IReadOnlyList<IBuildTarget> Resolve(string[] requested) {
        if (requested.Length == 0) {
            return All;
        }

        Dictionary<string, IBuildTarget> map = All.ToDictionary(target => target.Name, StringComparer.OrdinalIgnoreCase);
        var resolved = new List<IBuildTarget>(requested.Length);
        foreach (string raw in requested) {
            string normalized = raw.Replace('-', '_');
            if (!map.TryGetValue(normalized, out IBuildTarget? target)) {
                throw new Exception($"unknown target '{raw}'");
            }
            resolved.Add(target);
        }
        return resolved;
    }
}
