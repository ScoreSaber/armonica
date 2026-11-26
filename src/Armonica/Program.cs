using System.CommandLine;
using Armonica;
using Armonica.Targets;
using Microsoft.Extensions.Logging;

var depsOption = new Option<string?>("--deps-dir") { Description = "dependency cache directory", Recursive = true };
var protonOption = new Option<string?>("--proton") { Description = "proton tag to build against", Recursive = true };
var verboseOption = new Option<bool>("-v", "--verbose") { Description = "verbose logging", Recursive = true };

var root = new RootCommand("build armonica arm64 runtime dlls") {
    depsOption,
    protonOption,
    verboseOption
};

var listCommand = new Command("list", "list build targets");
listCommand.SetAction(_ => {
    foreach (IBuildTarget target in TargetRegistry.All) {
        Console.WriteLine(target.Name);
    }
});

var targetsArgument = new Argument<string[]>("targets") { Arity = ArgumentArity.ZeroOrMore };
var buildCommand = new Command("build", "build runtime dlls") { targetsArgument };
buildCommand.SetAction(async parseResult => {
    using ILoggerFactory loggerFactory = CreateLoggerFactory(parseResult);
    ILogger log = loggerFactory.CreateLogger("armonica");
    BuildContext context = CreateContext(parseResult, loggerFactory);

    foreach (IBuildTarget target in TargetRegistry.Resolve(parseResult.GetValue(targetsArgument) ?? [])) {
        using (log.BeginScope(target.Name)) {
            log.LogInformation("building");
            await target.BuildAsync(context);
        }
    }
});

var verifyCommand = new Command("verify", "check built dlls");
verifyCommand.SetAction(parseResult => {
    using ILoggerFactory loggerFactory = CreateLoggerFactory(parseResult);
    Packager.VerifyOutputs(CreateContext(parseResult, loggerFactory));
    loggerFactory.CreateLogger("armonica").LogInformation("looks good");
});

var packageCommand = new Command("package", "bundle release tarball");
packageCommand.SetAction(parseResult => {
    using ILoggerFactory loggerFactory = CreateLoggerFactory(parseResult);
    Console.WriteLine(Packager.BuildPackage(CreateContext(parseResult, loggerFactory)));
});

root.Subcommands.Add(listCommand);
root.Subcommands.Add(buildCommand);
root.Subcommands.Add(verifyCommand);
root.Subcommands.Add(packageCommand);

try {
    return await root.Parse(args).InvokeAsync();
} catch (Exception ex) {
    Console.Error.WriteLine($"error: {ex.Message}");
    return 1;
}

ILoggerFactory CreateLoggerFactory(ParseResult parseResult) =>
    LoggerFactory.Create(builder =>
        builder.AddFilter(null, parseResult.GetValue(verboseOption) ? LogLevel.Trace : LogLevel.Information)
            .AddSimpleConsole(options => {
                options.IncludeScopes = true;
                options.SingleLine = true;
                options.TimestampFormat = "HH:mm:ss ";
            }));

BuildContext CreateContext(ParseResult parseResult, ILoggerFactory loggerFactory) =>
    BuildContext.Create(FindRepoRoot(), parseResult.GetValue(depsOption), parseResult.GetValue(protonOption), loggerFactory);

static string FindRepoRoot() {
    foreach (string start in new[] { Environment.CurrentDirectory, AppContext.BaseDirectory }) {
        for (DirectoryInfo? dir = new(start); dir != null; dir = dir.Parent) {
            if (File.Exists(Path.Combine(dir.FullName, "manifest.json"))) {
                return dir.FullName;
            }
        }
    }
    throw new Exception("couldn't find manifest.json");
}
