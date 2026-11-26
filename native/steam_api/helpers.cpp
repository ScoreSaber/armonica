#include "steam_api.hpp"

#include <cstring>
#include <new>

#include "sdk_inline.inc"

namespace {

ISteamNetworkingUtils* networkingUtils() {
    return SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
}

bool setConfig(ISteamNetworkingUtils* self, ESteamNetworkingConfigValue value,
               ESteamNetworkingConfigScope scope, intptr_t object,
               ESteamNetworkingConfigDataType type, const void* arg) {
    return SteamAPI_ISteamNetworkingUtils_SetConfigValue(self, value, scope, object, type, arg);
}

constexpr auto identityGoogleStadia = static_cast<ESteamNetworkingIdentityType>(19);

}

#define USER_ACCESSOR(type, name, version)                                                         \
    S_API type* name() {                                                                           \
        return static_cast<type*>(                                                                 \
            SteamInternal_FindOrCreateUserInterface(SteamAPI_GetHSteamUser(), version));           \
    }

#define GAMESERVER_ACCESSOR(type, name, version)                                                   \
    S_API type* name() {                                                                           \
        return static_cast<type*>(                                                                 \
            SteamInternal_FindOrCreateGameServerInterface(SteamGameServer_GetHSteamUser(),         \
                                                          version));                               \
    }

USER_ACCESSOR(ISteamApps, SteamAPI_SteamApps_v008, STEAMAPPS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamController, SteamAPI_SteamController_v008, STEAMCONTROLLER_INTERFACE_VERSION)
USER_ACCESSOR(ISteamFriends, SteamAPI_SteamFriends_v017, STEAMFRIENDS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamGameSearch, SteamAPI_SteamGameSearch_v001, STEAMGAMESEARCH_INTERFACE_VERSION)
USER_ACCESSOR(ISteamHTMLSurface, SteamAPI_SteamHTMLSurface_v005, STEAMHTMLSURFACE_INTERFACE_VERSION)
USER_ACCESSOR(ISteamHTTP, SteamAPI_SteamHTTP_v003, STEAMHTTP_INTERFACE_VERSION)
USER_ACCESSOR(ISteamInput, SteamAPI_SteamInput_v006, STEAMINPUT_INTERFACE_VERSION)
USER_ACCESSOR(ISteamInventory, SteamAPI_SteamInventory_v003, STEAMINVENTORY_INTERFACE_VERSION)
USER_ACCESSOR(ISteamMatchmaking, SteamAPI_SteamMatchmaking_v009, STEAMMATCHMAKING_INTERFACE_VERSION)
USER_ACCESSOR(ISteamMatchmakingServers, SteamAPI_SteamMatchmakingServers_v002,
              STEAMMATCHMAKINGSERVERS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamMusic, SteamAPI_SteamMusic_v001, STEAMMUSIC_INTERFACE_VERSION)
USER_ACCESSOR(ISteamMusicRemote, SteamAPI_SteamMusicRemote_v001, STEAMMUSICREMOTE_INTERFACE_VERSION)
USER_ACCESSOR(ISteamNetworking, SteamAPI_SteamNetworking_v006, STEAMNETWORKING_INTERFACE_VERSION)
USER_ACCESSOR(ISteamNetworkingMessages, SteamAPI_SteamNetworkingMessages_SteamAPI_v002,
              STEAMNETWORKINGMESSAGES_INTERFACE_VERSION)
USER_ACCESSOR(ISteamNetworkingSockets, SteamAPI_SteamNetworkingSockets_SteamAPI_v012,
              STEAMNETWORKINGSOCKETS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamParentalSettings, SteamAPI_SteamParentalSettings_v001,
              STEAMPARENTALSETTINGS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamParties, SteamAPI_SteamParties_v002, STEAMPARTIES_INTERFACE_VERSION)
USER_ACCESSOR(ISteamRemotePlay, SteamAPI_SteamRemotePlay_v002, STEAMREMOTEPLAY_INTERFACE_VERSION)
USER_ACCESSOR(ISteamRemoteStorage, SteamAPI_SteamRemoteStorage_v016,
              STEAMREMOTESTORAGE_INTERFACE_VERSION)
USER_ACCESSOR(ISteamScreenshots, SteamAPI_SteamScreenshots_v003, STEAMSCREENSHOTS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamTimeline, SteamAPI_SteamTimeline_v004, STEAMTIMELINE_INTERFACE_VERSION)
USER_ACCESSOR(ISteamUGC, SteamAPI_SteamUGC_v020, STEAMUGC_INTERFACE_VERSION)
USER_ACCESSOR(ISteamUser, SteamAPI_SteamUser_v023, STEAMUSER_INTERFACE_VERSION)
USER_ACCESSOR(ISteamUserStats, SteamAPI_SteamUserStats_v013, STEAMUSERSTATS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamUtils, SteamAPI_SteamUtils_v010, STEAMUTILS_INTERFACE_VERSION)
USER_ACCESSOR(ISteamVideo, SteamAPI_SteamVideo_v007, STEAMVIDEO_INTERFACE_VERSION)

S_API ISteamNetworkingUtils* SteamAPI_SteamNetworkingUtils_SteamAPI_v004() {
    return static_cast<ISteamNetworkingUtils*>(
        SteamInternal_CreateInterface(STEAMNETWORKINGUTILS_INTERFACE_VERSION));
}

GAMESERVER_ACCESSOR(ISteamGameServer, SteamAPI_SteamGameServer_v015,
                    STEAMGAMESERVER_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamGameServerStats, SteamAPI_SteamGameServerStats_v001,
                    STEAMGAMESERVERSTATS_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamHTTP, SteamAPI_SteamGameServerHTTP_v003, STEAMHTTP_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamInventory, SteamAPI_SteamGameServerInventory_v003,
                    STEAMINVENTORY_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamNetworking, SteamAPI_SteamGameServerNetworking_v006,
                    STEAMNETWORKING_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamNetworkingMessages, SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002,
                    STEAMNETWORKINGMESSAGES_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamNetworkingSockets, SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012,
                    STEAMNETWORKINGSOCKETS_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamUGC, SteamAPI_SteamGameServerUGC_v020, STEAMUGC_INTERFACE_VERSION)
GAMESERVER_ACCESSOR(ISteamUtils, SteamAPI_SteamGameServerUtils_v010, STEAMUTILS_INTERFACE_VERSION)

S_API void SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(ISteamNetworkingUtils* self) {
    SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate(self, 1e10f);
}

S_API bool SteamAPI_ISteamNetworkingUtils_IsFakeIPv4(ISteamNetworkingUtils* self, uint32 ipv4) {
    return SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType(self, ipv4) >
           k_ESteamNetworkingFakeIPType_NotFake;
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32(
    ISteamNetworkingUtils* self, ESteamNetworkingConfigValue value, int32 val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Global, 0,
                     k_ESteamNetworkingConfig_Int32, &val);
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat(
    ISteamNetworkingUtils* self, ESteamNetworkingConfigValue value, float val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Global, 0,
                     k_ESteamNetworkingConfig_Float, &val);
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString(
    ISteamNetworkingUtils* self, ESteamNetworkingConfigValue value, const char* val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Global, 0,
                     k_ESteamNetworkingConfig_String, val);
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr(
    ISteamNetworkingUtils* self, ESteamNetworkingConfigValue value, void* val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Global, 0,
                     k_ESteamNetworkingConfig_Ptr, &val);
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32(
    ISteamNetworkingUtils* self, HSteamNetConnection conn, ESteamNetworkingConfigValue value,
    int32 val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Connection, conn,
                     k_ESteamNetworkingConfig_Int32, &val);
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat(
    ISteamNetworkingUtils* self, HSteamNetConnection conn, ESteamNetworkingConfigValue value,
    float val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Connection, conn,
                     k_ESteamNetworkingConfig_Float, &val);
}

S_API bool SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString(
    ISteamNetworkingUtils* self, HSteamNetConnection conn, ESteamNetworkingConfigValue value,
    const char* val) {
    return setConfig(self, value, k_ESteamNetworkingConfig_Connection, conn,
                     k_ESteamNetworkingConfig_String, val);
}

#define GLOBAL_CALLBACK(name, fnType, config)                                                      \
    S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_##name(                            \
        ISteamNetworkingUtils* self, fnType fn) {                                                  \
        return SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr(                             \
            self, config, reinterpret_cast<void*>(fn));                                            \
    }

GLOBAL_CALLBACK(SteamNetConnectionStatusChanged, FnSteamNetConnectionStatusChanged,
                k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged)
GLOBAL_CALLBACK(SteamNetAuthenticationStatusChanged, FnSteamNetAuthenticationStatusChanged,
                k_ESteamNetworkingConfig_Callback_AuthStatusChanged)
GLOBAL_CALLBACK(SteamRelayNetworkStatusChanged, FnSteamRelayNetworkStatusChanged,
                k_ESteamNetworkingConfig_Callback_RelayNetworkStatusChanged)
GLOBAL_CALLBACK(FakeIPResult, FnSteamNetworkingFakeIPResult,
                k_ESteamNetworkingConfig_Callback_FakeIPResult)
GLOBAL_CALLBACK(MessagesSessionRequest, FnSteamNetworkingMessagesSessionRequest,
                k_ESteamNetworkingConfig_Callback_MessagesSessionRequest)
GLOBAL_CALLBACK(MessagesSessionFailed, FnSteamNetworkingMessagesSessionFailed,
                k_ESteamNetworkingConfig_Callback_MessagesSessionFailed)

S_API bool SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct(
    ISteamNetworkingUtils* self, const SteamNetworkingConfigValue_t& opt,
    ESteamNetworkingConfigScope scope, intptr_t scopeObj) {
    const void* val = opt.m_eDataType == k_ESteamNetworkingConfig_String
                          ? static_cast<const void*>(opt.m_val.m_string)
                          : static_cast<const void*>(&opt.m_val);
    return setConfig(self, opt.m_eValue, scope, scopeObj, opt.m_eDataType, val);
}

S_API void SteamAPI_SteamNetworkingIPAddr_Clear(SteamNetworkingIPAddr* self) { self->Clear(); }
S_API bool SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros(SteamNetworkingIPAddr* self) { return self->IsIPv6AllZeros(); }
S_API void SteamAPI_SteamNetworkingIPAddr_SetIPv6(SteamNetworkingIPAddr* self, const uint8* ipv6, uint16 port) { self->SetIPv6(ipv6, port); }
S_API void SteamAPI_SteamNetworkingIPAddr_SetIPv4(SteamNetworkingIPAddr* self, uint32 ip, uint16 port) { self->SetIPv4(ip, port); }
S_API bool SteamAPI_SteamNetworkingIPAddr_IsIPv4(SteamNetworkingIPAddr* self) { return self->IsIPv4(); }
S_API uint32 SteamAPI_SteamNetworkingIPAddr_GetIPv4(SteamNetworkingIPAddr* self) { return self->GetIPv4(); }
S_API void SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost(SteamNetworkingIPAddr* self, uint16 port) { self->SetIPv6LocalHost(port); }
S_API bool SteamAPI_SteamNetworkingIPAddr_IsLocalHost(SteamNetworkingIPAddr* self) { return self->IsLocalHost(); }
S_API bool SteamAPI_SteamNetworkingIPAddr_IsEqualTo(SteamNetworkingIPAddr* self, const SteamNetworkingIPAddr& other) { return *self == other; }

S_API void SteamAPI_SteamNetworkingIPAddr_ToString(SteamNetworkingIPAddr* self, char* buf,
                                                   uint32 size, bool withPort) {
    SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString(networkingUtils(), *self, buf,
                                                                  size, withPort);
}

S_API bool SteamAPI_SteamNetworkingIPAddr_ParseString(SteamNetworkingIPAddr* self, const char* str) {
    return SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString(networkingUtils(), self,
                                                                            str);
}

S_API ESteamNetworkingFakeIPType SteamAPI_SteamNetworkingIPAddr_GetFakeIPType(
    SteamNetworkingIPAddr* self) {
    return SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType(networkingUtils(),
                                                                              *self);
}

S_API bool SteamAPI_SteamNetworkingIPAddr_IsFakeIP(SteamNetworkingIPAddr* self) {
    return SteamAPI_SteamNetworkingIPAddr_GetFakeIPType(self) >
           k_ESteamNetworkingFakeIPType_NotFake;
}

S_API void SteamAPI_SteamNetworkingIdentity_Clear(SteamNetworkingIdentity* self) { self->Clear(); }
S_API bool SteamAPI_SteamNetworkingIdentity_IsInvalid(SteamNetworkingIdentity* self) { return self->IsInvalid(); }
S_API void SteamAPI_SteamNetworkingIdentity_SetSteamID(SteamNetworkingIdentity* self, uint64_steamid id) { self->SetSteamID64(id); }
S_API uint64_steamid SteamAPI_SteamNetworkingIdentity_GetSteamID(SteamNetworkingIdentity* self) { return self->GetSteamID64(); }
S_API void SteamAPI_SteamNetworkingIdentity_SetSteamID64(SteamNetworkingIdentity* self, uint64 id) { self->SetSteamID64(id); }
S_API uint64 SteamAPI_SteamNetworkingIdentity_GetSteamID64(SteamNetworkingIdentity* self) { return self->GetSteamID64(); }
S_API bool SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID(SteamNetworkingIdentity* self, const char* str) { return self->SetXboxPairwiseID(str); }
S_API const char* SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID(SteamNetworkingIdentity* self) { return self->GetXboxPairwiseID(); }

S_API void SteamAPI_SteamNetworkingIdentity_SetStadiaID(SteamNetworkingIdentity* self, uint64 id) {
    self->m_eType = identityGoogleStadia;
    self->m_cbSize = sizeof(self->data.m_steamID64);
    self->data.m_steamID64 = id;
}

S_API uint64 SteamAPI_SteamNetworkingIdentity_GetStadiaID(SteamNetworkingIdentity* self) {
    return self->m_eType == identityGoogleStadia ? self->data.m_steamID64 : 0;
}

S_API void SteamAPI_SteamNetworkingIdentity_SetPSNID(SteamNetworkingIdentity* self, uint64 id) { self->SetPSNID(id); }
S_API uint64 SteamAPI_SteamNetworkingIdentity_GetPSNID(SteamNetworkingIdentity* self) { return self->GetPSNID(); }
S_API void SteamAPI_SteamNetworkingIdentity_SetIPAddr(SteamNetworkingIdentity* self, const SteamNetworkingIPAddr& addr) { self->SetIPAddr(addr); }
S_API const SteamNetworkingIPAddr* SteamAPI_SteamNetworkingIdentity_GetIPAddr(SteamNetworkingIdentity* self) { return self->GetIPAddr(); }
S_API void SteamAPI_SteamNetworkingIdentity_SetIPv4Addr(SteamNetworkingIdentity* self, uint32 ipv4, uint16 port) { self->SetIPv4Addr(ipv4, port); }
S_API uint32 SteamAPI_SteamNetworkingIdentity_GetIPv4(SteamNetworkingIdentity* self) { return self->GetIPv4(); }
S_API void SteamAPI_SteamNetworkingIdentity_SetLocalHost(SteamNetworkingIdentity* self) { self->SetLocalHost(); }
S_API bool SteamAPI_SteamNetworkingIdentity_IsLocalHost(SteamNetworkingIdentity* self) { return self->IsLocalHost(); }
S_API bool SteamAPI_SteamNetworkingIdentity_SetGenericString(SteamNetworkingIdentity* self, const char* str) { return self->SetGenericString(str); }
S_API const char* SteamAPI_SteamNetworkingIdentity_GetGenericString(SteamNetworkingIdentity* self) { return self->GetGenericString(); }
S_API bool SteamAPI_SteamNetworkingIdentity_SetGenericBytes(SteamNetworkingIdentity* self, const void* data, uint32 len) { return self->SetGenericBytes(data, len); }
S_API const uint8* SteamAPI_SteamNetworkingIdentity_GetGenericBytes(SteamNetworkingIdentity* self, int& len) { return self->GetGenericBytes(len); }
S_API bool SteamAPI_SteamNetworkingIdentity_IsEqualTo(SteamNetworkingIdentity* self, const SteamNetworkingIdentity& other) { return *self == other; }

S_API ESteamNetworkingFakeIPType SteamAPI_SteamNetworkingIdentity_GetFakeIPType(
    SteamNetworkingIdentity* self) {
    const auto* addr = self->GetIPAddr();
    return addr != nullptr ? SteamAPI_SteamNetworkingIPAddr_GetFakeIPType(
                                 const_cast<SteamNetworkingIPAddr*>(addr))
                           : k_ESteamNetworkingFakeIPType_Invalid;
}

S_API bool SteamAPI_SteamNetworkingIdentity_IsFakeIP(SteamNetworkingIdentity* self) {
    return SteamAPI_SteamNetworkingIdentity_GetFakeIPType(self) >
           k_ESteamNetworkingFakeIPType_NotFake;
}

S_API void SteamAPI_SteamNetworkingIdentity_ToString(SteamNetworkingIdentity* self, char* buf,
                                                     uint32 size) {
    SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString(networkingUtils(), *self, buf,
                                                                    size);
}

S_API bool SteamAPI_SteamNetworkingIdentity_ParseString(SteamNetworkingIdentity* self,
                                                        const char* str) {
    return SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString(networkingUtils(),
                                                                              self, str);
}

S_API void SteamAPI_SteamNetworkingMessage_t_Release(SteamNetworkingMessage_t* self) { (*self->m_pfnRelease)(self); }
S_API void SteamAPI_SteamNetworkingConfigValue_t_SetInt32(SteamNetworkingConfigValue_t* self, ESteamNetworkingConfigValue val, int32_t data) { self->SetInt32(val, data); }
S_API void SteamAPI_SteamNetworkingConfigValue_t_SetInt64(SteamNetworkingConfigValue_t* self, ESteamNetworkingConfigValue val, int64_t data) { self->SetInt64(val, data); }
S_API void SteamAPI_SteamNetworkingConfigValue_t_SetFloat(SteamNetworkingConfigValue_t* self, ESteamNetworkingConfigValue val, float data) { self->SetFloat(val, data); }
S_API void SteamAPI_SteamNetworkingConfigValue_t_SetPtr(SteamNetworkingConfigValue_t* self, ESteamNetworkingConfigValue val, void* data) { self->SetPtr(val, data); }
S_API void SteamAPI_SteamNetworkingConfigValue_t_SetString(SteamNetworkingConfigValue_t* self, ESteamNetworkingConfigValue val, const char* data) { self->SetString(val, data); }

struct SteamDatagramHostedAddress {
    int size;
    char data[128];
};

S_API void SteamAPI_SteamDatagramHostedAddress_Clear(SteamDatagramHostedAddress* self) { std::memset(self, 0, sizeof(*self)); }
S_API SteamNetworkingPOPID SteamAPI_SteamDatagramHostedAddress_GetPopID(SteamDatagramHostedAddress*) { return 0; }
S_API void SteamAPI_SteamDatagramHostedAddress_SetDevAddress(SteamDatagramHostedAddress*, uint32, uint16, SteamNetworkingPOPID) {}

S_API bool SteamAPI_SteamIPAddress_t_IsSet(SteamIPAddress_t* self) { return self->IsSet(); }
S_API void SteamAPI_MatchMakingKeyValuePair_t_Construct(MatchMakingKeyValuePair_t* self) { new (self) MatchMakingKeyValuePair_t(); }
S_API void SteamAPI_servernetadr_t_Construct(servernetadr_t* self) { new (self) servernetadr_t(); }
S_API void SteamAPI_servernetadr_t_Init(servernetadr_t* self, unsigned int ip, uint16 queryPort, uint16 connPort) { self->Init(ip, queryPort, connPort); }
S_API uint16 SteamAPI_servernetadr_t_GetQueryPort(servernetadr_t* self) { return self->GetQueryPort(); }
S_API void SteamAPI_servernetadr_t_SetQueryPort(servernetadr_t* self, uint16 port) { self->SetQueryPort(port); }
S_API uint16 SteamAPI_servernetadr_t_GetConnectionPort(servernetadr_t* self) { return self->GetConnectionPort(); }
S_API void SteamAPI_servernetadr_t_SetConnectionPort(servernetadr_t* self, uint16 port) { self->SetConnectionPort(port); }
S_API uint32 SteamAPI_servernetadr_t_GetIP(servernetadr_t* self) { return self->GetIP(); }
S_API void SteamAPI_servernetadr_t_SetIP(servernetadr_t* self, uint32 ip) { self->SetIP(ip); }
S_API const char* SteamAPI_servernetadr_t_GetConnectionAddressString(servernetadr_t* self) { return self->GetConnectionAddressString(); }
S_API const char* SteamAPI_servernetadr_t_GetQueryAddressString(servernetadr_t* self) { return self->GetQueryAddressString(); }
S_API bool SteamAPI_servernetadr_t_IsLessThan(servernetadr_t* self, const servernetadr_t& other) { return *self < other; }
S_API void SteamAPI_servernetadr_t_Assign(servernetadr_t* self, const servernetadr_t& other) { *self = other; }
S_API void SteamAPI_gameserveritem_t_Construct(gameserveritem_t* self) { new (self) gameserveritem_t(); }
S_API const char* SteamAPI_gameserveritem_t_GetName(gameserveritem_t* self) { return self->GetName(); }
S_API void SteamAPI_gameserveritem_t_SetName(gameserveritem_t* self, const char* name) { self->SetName(name); }
