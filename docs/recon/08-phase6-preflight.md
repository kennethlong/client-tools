# 08 — Phase 6 Pre-flight Findings

> **Gathered:** 2026-05-05
> **Survey date:** 2026-05-05
> **swg-main commit at survey time:** 91f0357 ("Adjust the cfg if running Ubuntu")
>
> This document is consumed by the Phase 6 planning agent before planning begins.
> It covers two pre-flight tasks:
> - **P2.01** — sharedNetworkMessages divergence survey (wire format compatibility between whitengold and swg-main)
> - **P2.03** — swg-main LoginServer auth bypass mechanism (how community servers authenticate without SOE Station)

---

## P2.01 — sharedNetworkMessages divergence survey

### Scope

Comparison of `src/engine/shared/library/sharedNetworkMessages/src/shared/` between:
- **whitengold** — `D:/Code/swg-client/` (February 2015 leak)
- **swg-main** — `D:/Code/swg-main/` (last commit: 91f0357, 2020-08-26)

Scope per D-09: key divergences only; login-flow messages flagged explicitly. No line-by-line
diff of server-side-only message files — those are noise for Phase 6.

### Top-level directory comparison

| Entry | whitengold | swg-main | Delta |
|-------|-----------|---------|-------|
| `GrantCommand.cpp/.h` | present | present | identical |
| `GrantSkill.cpp/.h` | present | present | identical |
| `LogMessage.cpp/.h` | present | present | identical |
| `NameErrors.cpp/.h` | present | present | identical |
| `NetworkMessageFactory.cpp/.h` | present | present | identical |
| `RevokeCommand.cpp/.h` | present | present | identical |
| `RevokeSkill.cpp/.h` | present | present | identical |
| `Makefile.am` | present | **absent** | whitengold-only (autotools artifact) |
| `chat/` | present | present | shared subdir |
| `clientGameServer/` | present | present | shared subdir |
| `clientLoginServer/` | present | present | shared subdir |
| `common/` | present | present | shared subdir |
| `core/` | present | present | shared subdir |
| `customerService/` | present | present | shared subdir |
| `planetWatch/` | present | present | shared subdir |
| `voicechat/` | present | present | shared subdir |

**Summary:** Top-level files are identical in content. The only difference is `Makefile.am` present
in whitengold (Linux autotools build artifact) and absent in swg-main (which uses CMake).
All subdirectories are present in both trees.

### Login-flow message files (clientLoginServer/)

| File | whitengold | swg-main | Notes |
|------|-----------|---------|-------|
| `ClientLoginMessages.cpp` | present | present | login message implementations |
| `ClientLoginMessages.h` | present | present | **identical** — see structural comparison below |
| `LoginClusterStatus.cpp/.h` | present | present | cluster status message |
| `LoginClusterStatusEx.cpp/.h` | present | present | extended cluster status |
| `LoginEnumCluster.cpp/.h` | present | present | **identical** — see structural comparison below |
| `Makefile.am` | present | **absent** | autotools artifact only |

**File count:** whitengold has 9 entries (8 source files + Makefile.am); swg-main has 8 entries
(8 source files). The sole difference is the autotools Makefile.am.

### ClientLoginMessages.h — structural comparison

Both trees contain identical `ClientLoginMessages.h`. The following message types and classes
are defined:

**Constants (namespace `ClientLoginMessageConstants`):**
- `EnumerateCluster = 1`
- `ClientLoginId = 2`
- `LoginToken = 3`

**Message classes:**
- `LoginClientId` — sent by client to server; carries `id` (username), `key` (password), `version` (app version string)
- `LoginClientToken` — sent by server to client after validation; carries token bytes, `stationId` (uint32), `username`
- `LoginIncorrectClientId` — sent by server when version mismatch; carries `serverId` and `serverApplicationVersion`

The following message types are used in the Phase 6 `LoginClientId → LoginEnumCluster → EnumerateCharacterId` handshake:
`LoginClientId`, `LoginClientToken`, `LoginIncorrectClientId`, `LoginEnumCluster`.

**These files are identical between whitengold and swg-main.** No constructor signature changes,
no added/removed fields, no wire format differences.

### LoginEnumCluster.h — structural comparison

Both trees contain identical `LoginEnumCluster.h`:

```
struct LoginEnumCluster_ClusterData {
    uint32      m_clusterId;
    std::string m_clusterName;
    int         m_timeZone;
};

class LoginEnumCluster : public GameNetworkMessage {
    // members:
    Archive::AutoArray<ClusterData>  m_data;
    Archive::AutoVariable<int>       m_maxCharactersPerAccount;
};
```

Archive serialization order (wire format): `m_clusterId` → `m_clusterName` → `m_timeZone` for
each cluster entry; `m_maxCharactersPerAccount` as the envelope field. This is byte-for-byte
the same in both trees.

**LoginEnumCluster.h is identical between whitengold and swg-main. No wire format changes.**

### swgSharedNetworkMessages comparison

`src/game/shared/library/swgSharedNetworkMessages/src/shared/` subdirectories:

| Subdir | whitengold | swg-main | Notes |
|--------|-----------|---------|-------|
| `combat/` | present | present | combat messages |
| `consent/` | present | present | consent system |
| `core/` | present | present | core game messages |
| `permissionList/` | present | present | permission list messages |
| `survey/` | present | present | survey/resource scan |
| `Makefile.am` (file) | present | absent | autotools artifact only |

No login-relevant messages live in `swgSharedNetworkMessages` — this library carries
game-side messages (combat, resource survey, consent) not the login handshake.

### swgServerNetworkMessages (swg-main only)

`D:/Code/swg-main/src/game/server/library/swgServerNetworkMessages/` **exists** in swg-main.
It is absent from whitengold (client-only leak — server-side network message libraries were not included).

Top-level `src/shared/` subdirs: `combat/`, `core/`, `jedi/`, `money/`, `resource/`, `travel/`.

These are server-to-server messages (not client-facing). They are out of scope for Phase 6
client login work but are noted here so the Phase 6 planner knows the library exists.

### Phase 6 implications

- **Wire format is identical.** Both whitengold client and swg-main LoginServer use the same
  `ClientLoginMessages.h` and `LoginEnumCluster.h` definitions with no structural differences.
  The login handshake message serialization is byte-for-byte compatible — no protocol adaptation
  required for Phase 6.
- **No version negotiation risk from sharedNetworkMessages.** The `LoginIncorrectClientId`
  message can only be triggered by a `NetworkVersionId` mismatch, not by message struct changes.
  Phase 6 should verify that `GameNetworkMessage::NetworkVersionId` matches between whitengold
  client binary and swg-main LoginServer, but the message struct itself is not the risk.
- **swgSharedNetworkMessages subdirs are identical.** No game-side login messages differ;
  the only artifacts absent in swg-main are `Makefile.am` files (autotools build artifacts,
  not code).

---

## P2.03 — swg-main LoginServer auth bypass mechanism

### Background

The original SWG LoginServer authenticated clients via Sony's Station session service:
`sdt-session1.station.sony.com:3000`. This endpoint no longer exists. The whitengold
leak config (`exe/linux/loginServer.cfg`) has `validateStationKey=1` pointing at these
defunct SOE servers.

The swg-main community fork has replaced this auth path. The replacement mechanism is
documented below based on direct reading of swg-main source at commit 91f0357.

### swg-main auth decision tree

The `validateClient()` method in `ClientConnection.cpp` implements the following logic:

1. Client sends `LoginClientId` message containing `id` (username), `key` (password), and `version`.
2. Server receives `LoginClientId`, calls `validateClient(id, key)`.
3. `validateClient` reads `ConfigLoginServer::getExternalAuthUrl()`.
4. **If `externalAuthUrl` is EMPTY (the default):** `authOK = 1` unconditionally — any
   username/password pair is accepted without contacting any external service.
5. If `externalAuthUrl` is set and `useJsonWebApi=true`: POST JSON body to the auth
   endpoint with fields `user_name`, `user_password`, `stationID`, `ip`, `secretKey`.
   If the response JSON has `"message": "success"`, `authOK = 1`.
6. If `externalAuthUrl` is set and `useJsonWebApi=false`: POST form-encoded body
   (`user_name=...&user_password=...&stationID=...&ip=...&secretKey=...`) to the auth
   endpoint. If the response body is the string `"success"`, `authOK = 1`.
7. If `authOK`: calls `LoginServer::getInstance().onValidateClient(suid, username, this, true, NULL, 0xFFFFFFFF, 0xFFFFFFFF)`,
   which proceeds with the cluster enumeration / character list flow.

**Key insight for Phase 6 (StellaBellum-style VM):** The StellaBellum VM leaves
`externalAuthUrl` empty (the default). This means every login attempt succeeds
unconditionally — any username and password are accepted. No additional auth server
is needed to connect a whitengold client to a StellaBellum VM.

Note: the old `validateStationKey` code path is **commented out** in swg-main's
`onConnectionClosed()`:
```cpp
/* if ((ConfigLoginServer::getValidateStationKey() || ConfigLoginServer::getDoSessionLogin()) && !m_isValidated) {
     SessionApiClient *session = LoginServer::getInstance().getSessionApiClient();
     if (session) { session->dropClient(this); }
 }*/
```
The `validateStationKey` config key still exists but the code that acted on it is inert.

### ConfigLoginServer defaults (swg-main)

Source file: `src/engine/server/application/LoginServer/src/shared/ConfigLoginServer.cpp`

| Config Key | Default | Notes |
|------------|---------|-------|
| `validateStationKey` | `false` | Dead key — old Station validation code is commented out; setting true has no effect |
| `doSessionLogin` | `false` | Dead key — old session login path; commented out alongside validateStationKey |
| `externalAuthURL` | `""` (empty string) | **When empty: all logins succeed unconditionally** — this is the StellaBellum default |
| `useJsonWebApi` | `false` | Only relevant when `externalAuthURL` is set; selects JSON vs form-encoded POST |
| `useExternalAuth` | `false` | Gate flag for the external auth path |
| `externalAuthSecretKey` | `""` (empty string) | Shared secret passed to auth endpoint; default is empty |
| `clientServicePort` | `44453` | The port the client connects to for login |
| `sessionServers` | `"localhost:3004"` | Community default; replaces old SOE session server value |

### Historical whitengold config (for reference only)

From `exe/linux/loginServer.cfg` in the whitengold leak:

```
validateStationKey=1
doConsumption=1
sessionServer=sdt-session1.station.sony.com:3000
sessionServer=sdt-session3.station.sony.com:3000
DSN=sdswgp5b
databaseUID=login
databasePWD=login_prod8
privateIpMask=64.37.133
privateIpMask=64.37.145
...
```

**These are historical artifacts from the 2015 leak of the live SOE production configuration.
They are NOT live credentials.** The session endpoints (`sdt-session*.station.sony.com:3000`)
no longer exist — Sony's Station auth service has been offline since SWG shut down in 2011.
The Oracle DSN `sdswgp5b` references an internal SOE database that no longer exists.
The private IP masks (`64.37.*`) are historical SOE datacenter ranges with no relevance to
community setups. Do not treat any of these values as usable connection parameters.

### Client-side requirements for Phase 6

The swg-main LoginServer requires the following in the whitengold client's `client.cfg`
(specifically the config file placed in `bin/Debug/` or `bin/Release/` alongside the exe):

```ini
[ClientGame]
loginServerAddress0=<swg-main VM IP or 127.0.0.1>
loginServerPort0=44453

[Station]
# sessionId must NOT be set — leaving it unset avoids the dead SOE Station SSO
# code path in the client, which would attempt to contact sdt-session*.station.sony.com.
# gameFeatures=15 is typically set elsewhere in client.cfg (all four SKU bits active).
```

**Authentication credentials:** Any username and any password will be accepted by the
swg-main LoginServer when `externalAuthURL` is empty (the default for StellaBellum VMs).
The username is hashed via `std::hash<std::string>` to derive a `stationId` (uint32) for
the session — numeric usernames are passed through `atoi` directly, string usernames are
hashed. This stationId is used as the account identity within the cluster.

**Critical:** Do NOT set `[Station] sessionId` in client.cfg. That config entry activates
a dead code path in the client that attempts Station SSO against defunct SOE endpoints.
Leaving it unset causes the client to use the standard username/password login flow
(`LoginClientId` message), which is what swg-main's `validateClient()` expects.

### Phase 6 implications

- **The auth bypass is already in place in swg-main; no server changes are needed.**
  The StellaBellum VM default configuration (`externalAuthURL=""`) means any credentials
  work. Phase 6 can connect with any username/password combination.
- **The client must NOT send a `[Station] sessionId` config value.** That activates the
  defunct SOE Station SSO path in the client code. The correct login mode is the plain
  username/password `LoginClientId` message flow, which swg-main's LoginServer handles
  via `validateClient()`.
- **Phase 6 should verify `loginServerAddress0` and `loginServerPort0` resolve to the VM.**
  The default swg-main port is 44453 (confirmed from `ConfigLoginServer.cpp` line 70).
  If the VM is on a different host or port, update `client.cfg` accordingly before
  attempting the login handshake.
