#pragma once

#define STEAM_API_EXPORTS
#include "steam/steam_api_flat.h"
#include "steam/steam_gameserver.h"

namespace armonica {

inline void** vtableOf(void* object) {
    return *reinterpret_cast<void***>(object);
}

const char* mapInterfaceVersion(const char* version);
void* legacyUserInterface(const char* version);
void* rawGenericInterface(ISteamClient* self, HSteamUser user, HSteamPipe pipe, const char* version);
void patchWineFPCR();

extern const char* const interfaceVersions[];

}
