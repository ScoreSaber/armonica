namespace Armonica.Targets;

internal interface IBuildTarget {
    string Name { get; }
    Task BuildAsync(BuildContext context);
}
