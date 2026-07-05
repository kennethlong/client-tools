# Miles 7.2a retail runtime

Vendored 2026-07-05 (CONSULT-63) from the retail SWG install at `D:\Code\SWGSource Client v3.0` (mss32.dll 7.2a + its miles/ provider set -- the runtime every shipping SWG 32-bit client uses: retail, Restoration, Beyond).

Why not the in-repo 7.2e SDK redist: its mss32.dll FAILS MP3 ASI provider discovery (harness-proven: 'Error getting sound format' with ANY provider set, while this 7.2a core works with ANY provider set -- including 7.2e's). Compile/link stays on the miles-7.2e SDK (headers + Mss32.lib import lib); the 7.2a DLL exports all 59 functions the exe imports (dumpbin-verified).

