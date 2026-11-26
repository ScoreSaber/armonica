using System.Buffers.Binary;

namespace Armonica;

internal static class PEImage {
    private const ushort MachineARM64 = 0xAA64;
    private const ushort PE32PlusMagic = 0x20B;
    private const int CLRDirectoryIndex = 14;
    private static readonly byte[] WineBuiltinMarker = "Wine builtin DLL\0"u8.ToArray();

    internal static (bool IsARM64, bool IsWineBuiltin, bool IsManaged) Inspect(string path) {
        byte[] header = new byte[4096];
        using FileStream stream = File.OpenRead(path);
        int read = stream.Read(header, 0, header.Length);
        if (read < 0x40) {
            return default;
        }

        int peOffset = (int)BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(0x3C));
        if (peOffset + 26 > read || !header.AsSpan(peOffset, 4).SequenceEqual("PE\0\0"u8)) {
            return default;
        }

        ushort machine = BinaryPrimitives.ReadUInt16LittleEndian(header.AsSpan(peOffset + 4));
        ushort magic = BinaryPrimitives.ReadUInt16LittleEndian(header.AsSpan(peOffset + 24));
        int clrDirectory = peOffset + 24 + (magic == PE32PlusMagic ? 112 : 96) + CLRDirectoryIndex * 8;
        bool managed = clrDirectory + 4 <= read && BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(clrDirectory)) != 0;
        bool builtin = header.AsSpan(0x40, WineBuiltinMarker.Length).SequenceEqual(WineBuiltinMarker);
        return (machine == MachineARM64 && magic == PE32PlusMagic, builtin, managed);
    }

    internal static void MarkWineBuiltin(string path) {
        byte[] data = File.ReadAllBytes(path);
        uint peOffset = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(0x3C));
        if (peOffset < 0x60) {
            throw new Exception($"{Path.GetFileName(path)}: dos stub too small for wine builtin marker");
        }

        Array.Clear(data, 0x40, 32);
        "Wine builtin DLL"u8.CopyTo(data.AsSpan(0x40));
        File.WriteAllBytes(path, data);
    }
}
