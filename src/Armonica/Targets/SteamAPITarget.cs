using Armonica.Codegen;
using CliWrap;

namespace Armonica.Targets;

internal sealed class SteamAPITarget : IBuildTarget {
    public string Name => "steam_api";

    public async Task BuildAsync(BuildContext context) {
        string srcDir = Path.Combine(context.Native, "steam_api");
        string genDir = Path.Combine(context.Generated, "steam_api");
        string objDir = Path.Combine(context.Obj, "steam_api");
        string incDir = Path.Combine(objDir, "inc");
        string sdkDir = Path.Combine(context.ProtonDir, "lsteamclient", "steamworks_sdk_161");
        Directory.CreateDirectory(incDir);

        string link = Path.Combine(incDir, "steam");
        if (Directory.Exists(link) || File.Exists(link)) {
            Directory.Delete(link);
        }
        Directory.CreateSymbolicLink(link, sdkDir);

        SteamAPICodegen.Generate(Path.Combine(context.ProtonDir, "lsteamclient"), genDir);

        string[] flags = [
            "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
            "-ffunction-sections", "-fdata-sections",
            $"-I{incDir}", $"-I{srcDir}", $"-I{genDir}", $"-I{context.Native}",
            "-Wall", "-Wextra",
            "-Wno-unused-parameter", "-Wno-unused-function",
            "-Wno-pragma-pack", "-Wno-unknown-pragmas",
        ];

        (string Src, string Obj)[] units = [
            (Path.Combine(genDir, "flat.cpp"), Path.Combine(objDir, "flat.o")),
            (Path.Combine(srcDir, "core.cpp"), Path.Combine(objDir, "core.o")),
            (Path.Combine(srcDir, "helpers.cpp"), Path.Combine(objDir, "helpers.o")),
            (Path.Combine(srcDir, "fpcr.cpp"), Path.Combine(objDir, "fpcr.o")),
        ];

        foreach ((string src, string obj) in units) {
            await Cli.Wrap(context.Cxx).WithArguments(["-c", .. flags, src, "-o", obj]).ExecuteFallible();
        }

        string outDll = Path.Combine(context.Out, "steam_api64.dll");
        await Cli.Wrap(context.Cxx)
            .WithArguments(["-shared", "-Os", "-s", "-static", "-Wl,--gc-sections", "-o", outDll, .. units.Select(unit => unit.Obj)])
            .ExecuteFallible();
    }
}
