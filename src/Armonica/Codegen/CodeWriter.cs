using System.CodeDom.Compiler;

namespace Armonica.Codegen;

internal sealed class CodeWriter : IDisposable {
    private readonly StringWriter _buffer = new();
    private readonly IndentedTextWriter _writer;

    internal CodeWriter() {
        _writer = new IndentedTextWriter(_buffer, "    ");
    }

    internal void Line(string text) => _writer.WriteLine(text);

    internal void Blank() => _writer.WriteLineNoTabs(string.Empty);

    internal IDisposable Block(string header, string close = "}") {
        _writer.WriteLine($"{header} {{");
        _writer.Indent++;
        return new Scope(_writer, close);
    }

    internal void Save(string path) {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, _buffer.ToString());
    }

    public void Dispose() {
        _writer.Dispose();
        _buffer.Dispose();
    }

    private sealed class Scope(IndentedTextWriter writer, string close) : IDisposable {
        public void Dispose() {
            writer.Indent--;
            writer.WriteLine(close);
        }
    }
}
