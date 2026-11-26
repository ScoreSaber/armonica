using System.Text;

namespace Armonica.Codegen;

internal static class SteamAPICodegen {
    private sealed record FlatMethod(string FlatName, string ClassName, string MethodName, string ReturnType, string Parameters);
    private sealed record LegacyMethod(string FlatName, string ClassName, string Version, string MethodName, string ReturnType, string Parameters, bool UseSelf);

    private static readonly Dictionary<string, string> InterfaceVersions = new(StringComparer.Ordinal) {
        ["ISteamApps"] = "STEAMAPPS_INTERFACE_VERSION008",
        ["ISteamClient"] = "SteamClient021",
        ["ISteamController"] = "SteamController008",
        ["ISteamFriends"] = "SteamFriends017",
        ["ISteamGameSearch"] = "SteamMatchGameSearch001",
        ["ISteamGameServer"] = "SteamGameServer015",
        ["ISteamGameServerStats"] = "SteamGameServerStats001",
        ["ISteamHTMLSurface"] = "STEAMHTMLSURFACE_INTERFACE_VERSION_005",
        ["ISteamHTTP"] = "STEAMHTTP_INTERFACE_VERSION003",
        ["ISteamInput"] = "SteamInput006",
        ["ISteamInventory"] = "STEAMINVENTORY_INTERFACE_V003",
        ["ISteamMatchmaking"] = "SteamMatchMaking009",
        ["ISteamMatchmakingServers"] = "SteamMatchMakingServers002",
        ["ISteamMusic"] = "STEAMMUSIC_INTERFACE_VERSION001",
        ["ISteamMusicRemote"] = "STEAMMUSICREMOTE_INTERFACE_VERSION001",
        ["ISteamNetworking"] = "SteamNetworking006",
        ["ISteamNetworkingFakeUDPPort"] = "SteamNetworkingFakeUDPPort001",
        ["ISteamNetworkingMessages"] = "SteamNetworkingMessages002",
        ["ISteamNetworkingSockets"] = "SteamNetworkingSockets012",
        ["ISteamNetworkingUtils"] = "SteamNetworkingUtils004",
        ["ISteamParentalSettings"] = "STEAMPARENTALSETTINGS_INTERFACE_VERSION001",
        ["ISteamParties"] = "SteamParties002",
        ["ISteamRemotePlay"] = "STEAMREMOTEPLAY_INTERFACE_VERSION002",
        ["ISteamRemoteStorage"] = "STEAMREMOTESTORAGE_INTERFACE_VERSION016",
        ["ISteamScreenshots"] = "STEAMSCREENSHOTS_INTERFACE_VERSION003",
        ["ISteamTimeline"] = "STEAMTIMELINE_INTERFACE_V004",
        ["ISteamUGC"] = "STEAMUGC_INTERFACE_VERSION020",
        ["ISteamUser"] = "SteamUser023",
        ["ISteamUserStats"] = "STEAMUSERSTATS_INTERFACE_VERSION013",
        ["ISteamUtils"] = "SteamUtils010",
        ["ISteamVideo"] = "STEAMVIDEO_INTERFACE_V007",
    };

    private static readonly LegacyMethod[] LegacyMethods = [
        new("SteamAPI_ISteamAppList_GetAppBuildId", "ISteamAppList", "STEAMAPPLIST_INTERFACE_VERSION001", "GetAppBuildId", "int", "AppId_t nAppID", true),
        new("SteamAPI_ISteamAppList_GetAppInstallDir", "ISteamAppList", "STEAMAPPLIST_INTERFACE_VERSION001", "GetAppInstallDir", "int", "AppId_t nAppID, char* pchDirectory, int cchNameMax", true),
        new("SteamAPI_ISteamAppList_GetAppName", "ISteamAppList", "STEAMAPPLIST_INTERFACE_VERSION001", "GetAppName", "int", "AppId_t nAppID, char* pchName, int cchNameMax", true),
        new("SteamAPI_ISteamAppList_GetInstalledApps", "ISteamAppList", "STEAMAPPLIST_INTERFACE_VERSION001", "GetInstalledApps", "uint32", "AppId_t* pvecAppID, uint32 unMaxAppIDs", true),
        new("SteamAPI_ISteamAppList_GetNumInstalledApps", "ISteamAppList", "STEAMAPPLIST_INTERFACE_VERSION001", "GetNumInstalledApps", "uint32", "", true),
        new("SteamAPI_ISteamUserStats_RequestCurrentStats", "ISteamUserStats", "STEAMUSERSTATS_INTERFACE_VERSION012", "RequestCurrentStats", "bool", "", false),
    ];

    private static readonly HashSet<string> InlineUnionFields = new(StringComparer.Ordinal) {
        "m_PSNID",
        "m_genericBytes",
        "m_ip",
        "m_ipv4",
        "m_ipv6",
        "m_steamID64",
        "m_szGenericString",
        "m_szXboxPairwiseID",
    };

    internal static void Generate(string lscDir, string outDir) {
        string sdkDir = Path.Combine(lscDir, "steamworks_sdk_161");
        Directory.CreateDirectory(outDir);
        GenerateFlatWrappers(lscDir, sdkDir, Path.Combine(outDir, "flat.cpp"));
        GenerateSDKInline(sdkDir, Path.Combine(outDir, "sdk_inline.inc"));
    }

    private static void GenerateFlatWrappers(string lscDir, string sdkDir, string outFile) {
        List<FlatMethod> flatMethods = ParseFlatHeader(Path.Combine(sdkDir, "steam_api_flat.h"));
        (Dictionary<(string ClassName, string Version), List<string>> vtables, HashSet<string> retPtrs) = ParseLSteamClient(lscDir);
        Dictionary<(string ClassName, string FlatMethodName), string> overloads = ParseSDKOverloads(sdkDir);

        var byClass = new Dictionary<string, List<FlatMethod>>(StringComparer.Ordinal);
        var classOrder = new List<string>();
        foreach (FlatMethod method in flatMethods) {
            if (!byClass.TryGetValue(method.ClassName, out List<FlatMethod>? list)) {
                list = [];
                byClass[method.ClassName] = list;
                classOrder.Add(method.ClassName);
            }
            list.Add(method);
        }

        using var code = new CodeWriter();
        code.Line("#include \"steam_api.hpp\"");
        code.Blank();

        foreach (string className in classOrder) {
            List<FlatMethod> methods = byClass[className];
            InterfaceVersions.TryGetValue(className, out string? version);
            List<string> vtable = version == null
                ? methods.Select(method => method.MethodName).ToList()
                : vtables[(className, version)];

            foreach (FlatMethod method in methods) {
                string lscName = version != null && overloads.TryGetValue((className, method.MethodName), out string? mapped)
                    ? mapped
                    : method.MethodName;
                int slot = vtable.IndexOf(lscName);
                if (slot < 0) {
                    continue;
                }

                string winName = $"win{className}_{version}_{lscName}";
                (List<string> paramTypes, List<string> paramNames) = ParseParameters(method.Parameters);
                string args = string.Join(", ", paramNames);
                string signature = $"{className}* self" + (method.Parameters.Length == 0 ? string.Empty : $", {method.Parameters}");
                string callArgs = "self" + (args.Length == 0 ? string.Empty : $", {args}");

                using (code.Block($"S_API {method.ReturnType} {method.FlatName}({signature})")) {
                    if (className == "ISteamClient" && paramNames.Contains("pchVersion", StringComparer.Ordinal)) {
                        code.Line("pchVersion = armonica::mapInterfaceVersion(pchVersion);");
                    }

                    if (retPtrs.Contains(winName)) {
                        string fnPtr = string.Join(", ", new[] { "void*", $"{method.ReturnType}*" }.Concat(paramTypes));
                        string retArgs = "self, &result" + (args.Length == 0 ? string.Empty : $", {args}");
                        code.Line($"{method.ReturnType} result;");
                        code.Line($"reinterpret_cast<void (*)({fnPtr})>(armonica::vtableOf(self)[{slot}])({retArgs});");
                        code.Line("return result;");
                    } else {
                        string fnPtr = string.Join(", ", new[] { "void*" }.Concat(paramTypes));
                        code.Line($"return reinterpret_cast<{method.ReturnType} (*)({fnPtr})>(armonica::vtableOf(self)[{slot}])({callArgs});");
                    }
                }
                code.Blank();
            }
        }

        using (code.Block("namespace armonica")) {
            code.Blank();
            using (code.Block("const char* const interfaceVersions[] =", "};")) {
                foreach (string version in InterfaceVersions.Values.Distinct(StringComparer.Ordinal).OrderBy(v => v, StringComparer.Ordinal)) {
                    code.Line($"\"{version}\",");
                }
                code.Line("nullptr,");
            }
            code.Blank();

            int genericSlot = vtables[("ISteamClient", InterfaceVersions["ISteamClient"])].IndexOf("GetISteamGenericInterface");
            using (code.Block("void* rawGenericInterface(ISteamClient* self, HSteamUser user, HSteamPipe pipe, const char* version)")) {
                code.Line($"return reinterpret_cast<void* (*)(void*, HSteamUser, HSteamPipe, const char*)>(vtableOf(self)[{genericSlot}])(self, user, pipe, version);");
            }
            code.Blank();
        }
        code.Blank();

        foreach (LegacyMethod legacy in LegacyMethods) {
            int slot = vtables[(legacy.ClassName, legacy.Version)].IndexOf(legacy.MethodName);
            (List<string> paramTypes, List<string> paramNames) = ParseParameters(legacy.Parameters);
            string target = legacy.UseSelf ? "self" : $"armonica::legacyUserInterface(\"{legacy.Version}\")";
            string fallback = legacy.ReturnType == "bool" ? "false" : "0";
            string signature = "void* self" + (legacy.Parameters.Length == 0 ? string.Empty : $", {legacy.Parameters}");
            string fnPtr = string.Join(", ", new[] { "void*" }.Concat(paramTypes));
            string callArgs = "obj" + (paramNames.Count == 0 ? string.Empty : $", {string.Join(", ", paramNames)}");

            using (code.Block($"S_API {legacy.ReturnType} {legacy.FlatName}({signature})")) {
                code.Line($"void* obj = {target};");
                using (code.Block("if (obj == nullptr)")) {
                    code.Line($"return {fallback};");
                }
                code.Line($"return reinterpret_cast<{legacy.ReturnType} (*)({fnPtr})>(armonica::vtableOf(obj)[{slot}])({callArgs});");
            }
            code.Blank();
        }

        code.Save(outFile);
    }

    private static List<FlatMethod> ParseFlatHeader(string headerPath) {
        var result = new List<FlatMethod>();
        foreach (string rawLine in File.ReadLines(headerPath)) {
            if (!rawLine.StartsWith("S_API ", StringComparison.Ordinal)) {
                continue;
            }

            int openParen = rawLine.IndexOf('(');
            int closeParen = rawLine.LastIndexOf(");", StringComparison.Ordinal);
            if (openParen < 0 || closeParen <= openParen) {
                continue;
            }

            string left = rawLine["S_API ".Length..openParen].Trim();
            int flatStart = left.IndexOf("SteamAPI_I", StringComparison.Ordinal);
            if (flatStart < 0) {
                continue;
            }

            string returnType = left[..flatStart].Trim();
            string flatName = left[flatStart..].Trim();
            string afterPrefix = flatName["SteamAPI_".Length..];
            int underscore = afterPrefix.IndexOf('_');
            if (underscore <= 0) {
                continue;
            }

            string className = afterPrefix[..underscore];
            string methodName = afterPrefix[(underscore + 1)..];
            string rawParams = rawLine[(openParen + 1)..closeParen].Trim();

            int comma = rawParams.IndexOf(',');
            string firstParam = (comma < 0 ? rawParams : rawParams[..comma]).Trim();
            if (!IsSelfParameter(firstParam, className)) {
                continue;
            }

            string restParams = comma < 0 ? string.Empty : rawParams[(comma + 1)..].Trim();
            result.Add(new FlatMethod(flatName, className, methodName, returnType, restParams));
        }
        return result;
    }

    private static bool IsSelfParameter(string param, string className) {
        if (!param.StartsWith(className, StringComparison.Ordinal)) {
            return false;
        }
        string tail = param[className.Length..].Trim();
        return tail.StartsWith('*') && tail[1..].Trim() == "self";
    }

    private static (Dictionary<(string, string), List<string>> Vtables, HashSet<string> RetPtrs) ParseLSteamClient(string lscDir) {
        var vtables = new Dictionary<(string, string), List<string>>();
        var retPtrs = new HashSet<string>(StringComparer.Ordinal);

        foreach (string path in Directory.GetFiles(lscDir, "winI*.c").OrderBy(p => p, StringComparer.Ordinal)) {
            string? currentVersion = null;
            List<string>? currentEntries = null;

            foreach (string line in File.ReadLines(path, Encoding.Latin1)) {
                int vtableStart = line.IndexOf("__ASM_VTABLE(winI", StringComparison.Ordinal);
                if (vtableStart >= 0) {
                    int nameStart = vtableStart + "__ASM_VTABLE(".Length;
                    int comma = line.IndexOf(',', nameStart);
                    if (comma > nameStart) {
                        string fullName = line[nameStart..comma].Trim();
                        int sep = fullName.IndexOf('_');
                        if (sep > 3) {
                            string className = fullName[3..sep];
                            currentVersion = fullName[(sep + 1)..];
                            currentEntries = [];
                            vtables[(className, currentVersion)] = currentEntries;
                        }
                    }
                    continue;
                }

                if (currentEntries != null) {
                    int addIndex = line.IndexOf("VTABLE_ADD_FUNC(", StringComparison.Ordinal);
                    if (addIndex >= 0) {
                        int fnStart = addIndex + "VTABLE_ADD_FUNC(".Length;
                        int fnEnd = line.IndexOf(')', fnStart);
                        if (fnEnd > fnStart) {
                            string symbol = line[fnStart..fnEnd].Trim();
                            string marker = $"_{currentVersion}_";
                            int markerPos = symbol.IndexOf(marker, StringComparison.Ordinal);
                            if (markerPos >= 0) {
                                currentEntries.Add(symbol[(markerPos + marker.Length)..]);
                            }
                        }
                    } else if (!string.IsNullOrWhiteSpace(line)) {
                        currentVersion = null;
                        currentEntries = null;
                    }
                }

                int thiscall = line.IndexOf("__thiscall winI", StringComparison.Ordinal);
                if (thiscall >= 0 && line.Contains("struct w_iface *_this,", StringComparison.Ordinal)) {
                    int openParen = line.IndexOf('(', thiscall);
                    int closeParen = openParen >= 0 ? line.IndexOf(')', openParen) : -1;
                    if (openParen > 0 && closeParen > openParen) {
                        string paramList = line[(openParen + 1)..closeParen];
                        if (paramList.Contains("*_ret,", StringComparison.Ordinal) || paramList.EndsWith("*_ret", StringComparison.Ordinal)) {
                            retPtrs.Add(line[(thiscall + "__thiscall ".Length)..openParen].Trim());
                        }
                    }
                }
            }
        }

        return (vtables, retPtrs);
    }

    private static Dictionary<(string, string), string> ParseSDKOverloads(string sdkDir) {
        var overloads = new Dictionary<(string, string), string>();

        foreach (string path in Directory.GetFiles(sdkDir, "*.h").OrderBy(p => p, StringComparer.Ordinal)) {
            string? currentClass = null;
            bool inClassBody = false;
            var counts = new Dictionary<string, int>(StringComparer.Ordinal);
            string? pendingFlat = null;

            foreach (string rawLine in File.ReadLines(path, Encoding.Latin1)) {
                if (currentClass == null) {
                    if (rawLine.StartsWith("class I", StringComparison.Ordinal)) {
                        string rest = rawLine["class ".Length..];
                        int end = 0;
                        while (end < rest.Length && IsIdentPart(rest[end])) {
                            end++;
                        }
                        if (end > 1 && rest[end..].Trim() is "" or "{") {
                            currentClass = rest[..end];
                            inClassBody = rawLine.Contains('{');
                            counts.Clear();
                            pendingFlat = null;
                        }
                    }
                    continue;
                }

                if (!inClassBody) {
                    if (rawLine.TrimStart().StartsWith('{')) {
                        inClassBody = true;
                    } else {
                        currentClass = null;
                    }
                    continue;
                }

                if (rawLine.StartsWith("};", StringComparison.Ordinal)) {
                    currentClass = null;
                    inClassBody = false;
                    pendingFlat = null;
                    continue;
                }

                int flatAttr = rawLine.IndexOf("STEAM_FLAT_NAME(", StringComparison.Ordinal);
                if (flatAttr >= 0) {
                    int start = flatAttr + "STEAM_FLAT_NAME(".Length;
                    int end = rawLine.IndexOf(')', start);
                    if (end > start) {
                        pendingFlat = rawLine[start..end].Trim();
                    }
                    continue;
                }

                string trimmed = rawLine.TrimStart();
                if (!trimmed.StartsWith("virtual ", StringComparison.Ordinal)) {
                    continue;
                }

                int openParen = trimmed.IndexOf('(');
                if (openParen <= 0) {
                    continue;
                }

                string methodName = ReadTrailingIdentifier(trimmed[..openParen]);
                if (methodName.Length == 0) {
                    continue;
                }

                int seen = counts.GetValueOrDefault(methodName) + 1;
                counts[methodName] = seen;
                overloads[(currentClass, pendingFlat ?? methodName)] = seen == 1 ? methodName : $"{methodName}_{seen}";
                pendingFlat = null;
            }
        }

        return overloads;
    }

    private static (List<string> Types, List<string> Names) ParseParameters(string parameters) {
        var types = new List<string>();
        var names = new List<string>();
        if (string.IsNullOrWhiteSpace(parameters)) {
            return (types, names);
        }

        foreach (string raw in parameters.Split(',')) {
            string item = raw.Trim();
            string name = ReadTrailingIdentifier(item);
            int pos = item.LastIndexOf(name, StringComparison.Ordinal);
            types.Add(item[..pos].Trim());
            names.Add(name);
        }
        return (types, names);
    }

    private static string ReadTrailingIdentifier(string text) {
        int end = text.Length;
        while (end > 0 && char.IsWhiteSpace(text[end - 1])) {
            end--;
        }
        int start = end;
        while (start > 0 && IsIdentPart(text[start - 1])) {
            start--;
        }
        return text[start..end];
    }

    private static void GenerateSDKInline(string sdkDir, string outFile) {
        string[] lines = File.ReadAllLines(Path.Combine(sdkDir, "steamnetworkingtypes.h"), Encoding.Latin1);
        int start = Array.FindIndex(lines, line => line.StartsWith("inline void SteamNetworkingIPAddr::Clear()", StringComparison.Ordinal));
        int end = Array.FindIndex(lines, line => line.Contains("SteamNetworkingIdentity::operator==", StringComparison.Ordinal));

        string body = string.Join('\n', lines[start..(end + 1)])
            .Replace("const void *data, size_t cbLen", "const void* pData, size_t cbLen", StringComparison.Ordinal)
            .Replace("memcpy( m_genericBytes, data,", "memcpy( m_genericBytes, pData,", StringComparison.Ordinal);

        body = RewriteUnionFields(body)
            .Replace("x.m_genericBytes", "x.data.m_genericBytes", StringComparison.Ordinal);

        File.WriteAllText(outFile, body + "\n");
    }

    private static string RewriteUnionFields(string source) {
        var result = new StringBuilder(source.Length + 256);
        int index = 0;
        while (index < source.Length) {
            if (!IsIdentStart(source[index])) {
                result.Append(source[index++]);
                continue;
            }

            int start = index;
            while (index < source.Length && IsIdentPart(source[index])) {
                index++;
            }

            string token = source[start..index];
            bool afterDot = start > 0 && source[start - 1] == '.';
            if (!afterDot && InlineUnionFields.Contains(token)) {
                result.Append("data.");
            }
            result.Append(token);
        }
        return result.ToString();
    }

    private static bool IsIdentStart(char c) => char.IsLetter(c) || c == '_';
    private static bool IsIdentPart(char c) => char.IsLetterOrDigit(c) || c == '_';
}
