using CliWrap;

namespace Armonica.Targets;

internal sealed class ProtonModuleTarget(string module) : IBuildTarget {
    public string Name => module;

    public async Task BuildAsync(BuildContext context) {
        string srcDir = Path.Combine(context.ProtonDir, module);
        string objDir = Path.Combine(context.Obj, module);
        string outName = $"{module}_a64.dll";
        string[] makefile = File.ReadAllLines(Path.Combine(srcDir, "Makefile.in"));
        string resourceDir = (await Cli.Wrap(context.Cc).WithArguments(["-print-resource-dir"]).ExecuteFallible()).StandardOutput.Trim();
        string[] flags = [
            "-O2", "-nostdinc", "-isystem", Path.Combine(resourceDir, "include"),
            "-I", srcDir,
            "-I", Path.Combine(context.WineDir, "include"),
            "-I", Path.Combine(context.WineDir, "include", "msvcrt"),
            "-I", Path.Combine(context.WineToolsDir, "include"),
            "-D__WINESRC__", "-D_UCRT", "-D_WIN32_WINNT=0xa00",
            "-DWINE_NO_TRACE_MSGS", "-fno-builtin", "-fms-extensions", "-Wno-everything",
            .. ReadVariable(makefile, "EXTRADEFS"),
        ];

        Directory.CreateDirectory(objDir);
        List<(string Source, string Object, string[] Flags)> units = Directory.GetFiles(srcDir, "*.c")
            .Where(IsPESource)
            .Order(StringComparer.Ordinal)
            .Select(source => (source, Path.Combine(objDir, $"{Path.GetFileName(source)}.o"), flags))
            .ToList();
        units.Add((Path.Combine(context.WineDir, "dlls", "winecrt0", "unix_lib.c"), Path.Combine(objDir, "unix_lib.o"), [.. flags, "-D__WINE_PE_BUILD"]));

        await Parallel.ForEachAsync(
            units,
            new ParallelOptions { MaxDegreeOfParallelism = context.Jobs },
            async (unit, _) => await Cli.Wrap(context.Cc).WithArguments(["-c", .. unit.Flags, unit.Source, "-o", unit.Object]).ExecuteFallible());

        string def = Path.Combine(objDir, $"{module}.def");
        await File.WriteAllLinesAsync(def, [$"LIBRARY {outName}", "EXPORTS", .. ReadExports(Path.Combine(srcDir, $"{module}.spec"))]);

        string[] imports = ["ntdll", .. ReadVariable(makefile, "IMPORTS")];
        var libraries = new List<string>();
        foreach (string import in imports) {
            libraries.Add(await ResolveImportLibAsync(context, import, objDir));
        }

        string outDll = Path.Combine(context.Out, outName);
        await Cli.Wrap(context.Cc)
            .WithArguments(["-shared", "-o", outDll, .. units.Select(unit => unit.Object), def, $"-L{objDir}", .. libraries])
            .ExecuteFallible();
        PEImage.MarkWineBuiltin(outDll);
    }

    private static string[] ReadVariable(string[] makefile, string name) =>
        makefile
            .FirstOrDefault(line => line.StartsWith(name, StringComparison.Ordinal) && line[name.Length..].TrimStart().StartsWith('='))
            ?.Split('=', 2)[1]
            .Split(' ', StringSplitOptions.RemoveEmptyEntries)
        ?? [];

    private static bool IsPESource(string path) =>
        !File.ReadLines(path).Take(40).Any(line => line.Contains("#pragma makedep unix", StringComparison.Ordinal));

    private static IEnumerable<string> ReadExports(string specPath) =>
        File.ReadLines(specPath)
            .Select(line => line.Split(' ', StringSplitOptions.RemoveEmptyEntries))
            .Where(parts => parts.Length >= 3 && parts[1] is "cdecl" or "stdcall")
            .Select(parts => parts.Skip(2).First(part => !part.StartsWith('-')).Split('(')[0]);

    private static async Task<string> ResolveImportLibAsync(BuildContext context, string import, string objDir) {
        bool wineOnly = import == "ntdll" || !File.Exists(Path.Combine(context.LLVMMingw, "aarch64-w64-mingw32", "lib", $"lib{import}.a"));
        if (!wineOnly) {
            return $"-l{import}";
        }

        string def = Path.Combine(objDir, $"{import}.def");
        string winebuild = Path.Combine(context.WineToolsDir, "tools", "winebuild", "winebuild");
        string spec = Path.Combine(context.WineDir, "dlls", import, $"{import}.spec");
        await Cli.Wrap(winebuild).WithArguments(["--def", "-m64", "--target", "aarch64-windows", "-E", spec, "-o", def]).ExecuteFallible();
        await Cli.Wrap(context.Dlltool).WithArguments(["-m", "arm64", "-d", def, "-l", Path.Combine(objDir, $"lib{import}_wine.a")]).ExecuteFallible();
        return $"-l{import}_wine";
    }
}
