namespace Armonica.Codegen;

internal static class DoorstopCodegen {
    private const string Marker = "ADD_ORIGINAL(";

    internal static void Generate(string proxySourcePath, string outputPath) {
        var exports = new List<(int Index, string Name)>();
        foreach (string rawLine in File.ReadLines(proxySourcePath)) {
            ReadOnlySpan<char> line = rawLine.AsSpan().Trim();
            if (!line.StartsWith(Marker, StringComparison.Ordinal)) {
                continue;
            }

            int close = line.IndexOf(')');
            if (close < 0) {
                continue;
            }

            ReadOnlySpan<char> inner = line[Marker.Length..close];
            int comma = inner.IndexOf(',');
            if (comma < 0) {
                continue;
            }

            int index = int.Parse(inner[..comma].Trim());
            string name = inner[(comma + 1)..].Trim().ToString();
            exports.Add((index, name));
        }

        using var code = new CodeWriter();
        code.Line("#include <windows.h>");
        code.Blank();
        code.Line($"FARPROC originalFunctions[{exports.Count}] = {{0}};");
        code.Blank();

        using (code.Block("void loadFunctions(HMODULE dll)")) {
            foreach ((int index, string name) in exports) {
                code.Line($"originalFunctions[{index}] = GetProcAddress(dll, \"{name}\");");
            }
        }
        code.Blank();

        foreach ((int index, string name) in exports) {
            code.Line(
                $"__asm__(\".globl {name}\\n.def {name}; .scl 2; .type 32; .endef\\n{name}:\\n" +
                $"\\tadrp x16, originalFunctions\\n\\tadd x16, x16, :lo12:originalFunctions\\n" +
                $"\\tldr x16, [x16, #{index * 8}]\\n\\tbr x16\\n\");");
        }

        code.Save(outputPath);
    }
}
