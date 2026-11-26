using System.Text.Json;

namespace Armonica;

internal sealed record Manifest(
    int Version,
    string Proton,
    string LLVMMingwVersion,
    ComponentsSpec Components,
    PackageSpec Package) {
    private static readonly JsonSerializerOptions JsonOptions = new() {
        PropertyNameCaseInsensitive = true
    };

    internal static Manifest Load(string path) =>
        JsonSerializer.Deserialize<Manifest>(File.ReadAllText(path), JsonOptions)
        ?? throw new Exception($"bad manifest: {path}");
}

internal sealed record ComponentsSpec(
    string ZlibVersion,
    string BSIPACommit,
    string MonoModCommit,
    string MonoModVersion,
    string MonoModSHA256,
    string UnityMonoCommit);

internal sealed record PackageSpec(int SteamAPIMaxBytes, SortedDictionary<string, string> Files);
