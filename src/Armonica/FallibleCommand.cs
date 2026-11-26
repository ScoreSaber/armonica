using System.Security.Cryptography;
using CliWrap;
using CliWrap.Buffered;

namespace Armonica;

internal static class FallibleCommand {
    private static readonly HttpClient Http = new();

    internal static async Task<BufferedCommandResult> ExecuteFallible(this Command command) {
        BufferedCommandResult result = await command.WithValidation(CommandResultValidation.None).ExecuteBufferedAsync();
        if (result.ExitCode != 0) {
            string err = string.IsNullOrWhiteSpace(result.StandardError) ? result.StandardOutput : result.StandardError;
            string tail = string.Join('\n', err.Trim().Split('\n').TakeLast(20));
            throw new Exception($"command failed ({result.ExitCode}): {command}\n{tail}".Trim());
        }
        return result;
    }

    internal static async Task DownloadAsync(string url, string destination) {
        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
        string temp = destination + ".part";
        try {
            using HttpResponseMessage response = await Http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead);
            response.EnsureSuccessStatusCode();
            await using Stream input = await response.Content.ReadAsStreamAsync();
            await using FileStream output = File.Create(temp);
            await input.CopyToAsync(output);
            output.Close();
            File.Move(temp, destination, overwrite: true);
        } catch {
            if (File.Exists(temp)) {
                File.Delete(temp);
            }
            throw;
        }
    }

    internal static async Task GitFetchCommitAsync(string url, string rev, string directory) {
        if (Directory.Exists(Path.Combine(directory, ".git"))) {
            return;
        }
        Directory.CreateDirectory(directory);
        await Cli.Wrap("git").WithArguments(["init", "-q", directory]).ExecuteFallible();
        await Cli.Wrap("git").WithArguments(["-C", directory, "remote", "add", "origin", url]).ExecuteFallible();
        await Cli.Wrap("git").WithArguments(["-C", directory, "fetch", "-q", "--depth", "1", "origin", rev]).ExecuteFallible();
        await Cli.Wrap("git").WithArguments(["-C", directory, "checkout", "-q", "FETCH_HEAD"]).ExecuteFallible();
    }

    internal static string SHA256File(string path) {
        using FileStream stream = File.OpenRead(path);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }

    internal static void CopyFile(string source, string destination) {
        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
        File.Copy(source, destination, overwrite: true);
    }
}
