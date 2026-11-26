using CliWrap;

namespace Armonica.Targets;

internal sealed class WineToolsTarget : IBuildTarget {
    private static readonly string[] ConfigureDisableFlags = [
        "--without-x", "--without-freetype", "--disable-tests", "--without-gstreamer",
        "--without-vulkan", "--without-wayland", "--without-alsa", "--without-pulse",
        "--without-dbus", "--without-gnutls", "--without-cups", "--without-sane",
        "--without-usb", "--without-v4l2", "--without-krb5", "--without-netapi",
        "--without-opencl", "--without-pcap", "--without-sdl", "--without-udev",
        "--without-unwind", "--without-gphoto", "--without-fontconfig",
    ];

    public string Name => "wine_tools";

    public async Task BuildAsync(BuildContext context) {
        string winebuild = Path.Combine(context.WineToolsDir, "tools", "winebuild", "winebuild");
        string d3d11 = Path.Combine(context.WineToolsDir, "include", "d3d11.h");
        if (File.Exists(winebuild) && File.Exists(d3d11)) {
            return;
        }

        if (!File.Exists(Path.Combine(context.WineDir, "configure"))) {
            await Cli.Wrap("autoreconf").WithArguments(["-f"]).WithWorkingDirectory(context.WineDir).ExecuteFallible();
        }

        await Cli.Wrap(Path.Combine(context.WineDir, "tools", "make_specfiles")).WithWorkingDirectory(context.WineDir).ExecuteFallible();
        await Cli.Wrap(Path.Combine(context.WineDir, "tools", "make_makefiles"))
            .WithWorkingDirectory(context.WineDir)
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync();
        await Cli.Wrap("python3")
            .WithArguments(["make_vulkan", "-x", "vk.xml", "-X", "video.xml"])
            .WithWorkingDirectory(Path.Combine(context.WineDir, "dlls", "winevulkan"))
            .ExecuteFallible();

        Directory.CreateDirectory(context.WineToolsDir);
        Dictionary<string, string?> env = context.ToolchainEnv();
        var configureArgs = new List<string> { "-q" };
        if (context.HostArch == "x86_64") {
            configureArgs.Add("--enable-win64");
        }
        configureArgs.AddRange(ConfigureDisableFlags);

        await Cli.Wrap(Path.Combine(context.WineDir, "configure"))
            .WithArguments(configureArgs)
            .WithWorkingDirectory(context.WineToolsDir)
            .WithEnvironmentVariables(env)
            .ExecuteFallible();

        await Cli.Wrap("make")
            .WithArguments(["-C", context.WineToolsDir, $"-j{context.Jobs}", "tools/widl/widl", "tools/winebuild/winebuild"])
            .WithEnvironmentVariables(env)
            .ExecuteFallible();

        string[] headers = ReadHeaderTargets(Path.Combine(context.WineToolsDir, "Makefile"));
        await Cli.Wrap("make")
            .WithArguments(["-C", context.WineToolsDir, "-k", $"-j{context.Jobs}", .. headers])
            .WithEnvironmentVariables(env)
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync();

        if (!File.Exists(d3d11)) {
            throw new Exception("wine headers failed to generate");
        }
    }

    private static string[] ReadHeaderTargets(string makefilePath) {
        var headers = new HashSet<string>(StringComparer.Ordinal);
        foreach (string line in File.ReadLines(makefilePath)) {
            if (!line.StartsWith("include/", StringComparison.Ordinal)) {
                continue;
            }
            int colon = line.IndexOf(':');
            if (colon <= "include/".Length) {
                continue;
            }
            string target = line[..colon];
            if (target.EndsWith(".h", StringComparison.Ordinal) && target != "include/config.h" && !target.Contains(' ')) {
                headers.Add(target);
            }
        }
        return headers.OrderBy(h => h, StringComparer.Ordinal).ToArray();
    }
}
