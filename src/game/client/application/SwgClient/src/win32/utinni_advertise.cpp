// ======================================================================
//
// utinni_advertise.cpp -- Utinni engine entry-point advertisement provider
// (handoff 2026-06-20). The exe-side game-logic twin of the shipped graphics
// gl11_r.dll!GetHookPoints (Direct3d11.cpp:856-888).
//
// Exports one undecorated extern "C" __cdecl GetEngineHookPoints() that hands
// an injected modding overlay (Utinni) a name->pointer table of engine
// functions/globals, each address taken at compile time by &EngineSymbol so
// it is correct by construction and survives every rebuild. The addresses are
// BORROWED (process-lifetime statics); Utinni only reads the table.
//
// Pure read-only getter -- no behavioral change, no Utinni dependency. If
// Utinni is not injected nothing calls GetEngineHookPoints(); the export is
// inert. Each row carries NO calling convention -- Utinni's typedef supplies
// it and must match MSVC's emitted convention.
//
// 32-bit only (spec 0 / 5): x64 is Utinni's deferred half. This TU introduces
// NO x64 export surface -- the whole body is guarded by #if !defined(_WIN64)
// (belt-and-suspenders with the vcxproj ClCompile Condition Platform=Win32).
//
// See .planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md.
// ======================================================================

#include "FirstSwgClient.h"

#include <bit>   // std::bit_cast (C++20, stdcpp20 enabled)

#include "utinni_engine_hookpoints.h"
#include "ClientMain.h"                            // utinni_installConfigFileOverride() + ClientMain()
#include "sharedFoundation/ConfigFile.h"           // ConfigFile::loadFile / loadFromBuffer (static)
#include "clientGame/Game.h"                       // Game::* (static) + isOver accessor
#include "clientGraphics/Graphics.h"               // Graphics::* (static)
#include "clientUserInterface/CuiManager.h"        // CuiManager::* (static) + getIoWin accessor
#include "clientUserInterface/CuiIoWin.h"          // CuiIoWin::* (member PMFs)
#include "clientUserInterface/CuiConsoleHelper.h"  // CuiConsoleHelper::processInput (member PMF)
#include "sharedCommandParser/CommandParser.h"     // CommandParser::addSubCommand (member PMF)
#include "clientGame/GroundScene.h"                 // GroundScene MI thunks + ctor (38-01)
#include "utinni_groundScene_forward.h"             // utinni_groundScene* private-method forwarders (38-01; exe-local)
#include "utinni_clientShims_forward.h"             // utinni_osWindowProc / utinni_writeMiniDump shims (38-02; exe-local)
#include "clientUserInterface/CuiPreferences.h"     // CuiPreferences::setModalChat/getModalChat (38-02; 37-02 CORRECTION -- NOT ConfigFile)
#include "swgClientUserInterface/SwgCuiChatWindow.h" // SwgCuiChatWindow MI thunks (38-03; TRIPLE-MI -> __fastcall thunks, never pmfToVoid)

// -- 37-03 full-catalog includes --------------------------------------------
#include "sharedCollision/BaseExtent.h"            // BaseExtent::intersect (non-virtual overload, §8 #2)
#include "sharedObject/Object.h"                   // Object non-virtual getters/setters (PMF)
#include "sharedGame/SharedObjectTemplate.h"       // SharedObjectTemplate filename getters (PMF) + ObjectTemplate::createObject
#include "clientGame/WorldSnapshot.h"              // WorldSnapshot::* (all static in this tree)
#include "clientGraphics/Camera.h"                 // Camera non-virtual setters (PMF)
#include "sharedMemoryManager/MemoryManager.h"     // MemoryManager::allocate/free (static)
#include "clientAudio/Audio.h"                     // Audio::set/getMasterVolume (static)
#include "sharedFile/TreeFile.h"                   // TreeFile::open (static)
#include "sharedDebug/Report.h"                    // Report::puts (static)

// 32-bit-only scope: compile this TU to nothing on x64. The vcxproj already
// conditions the ClCompile item to Platform=Win32; this guard is the source-
// side belt-and-suspenders so an x64 config can never export GetEngineHookPoints.
#if !defined(_WIN64)

// ----------------------------------------------------------------------
// PMF -> void* helper. Non-static, non-virtual member function pointers are
// not void*-convertible by a plain cast; std::bit_cast is the standard,
// well-defined C++20 idiom (a union type-pun would be ill-formed). The size
// guard catches MULTIPLE/VIRTUAL-INHERITANCE PMF inflation (>4 bytes) ONLY --
// it does NOT catch a &Class::virtualMethod (still pointer-sized, but yields a
// vtable-dispatch stub, NOT the impl). Virtual rows must be SKIPPED, not
// bit_cast (per the landmine rules in 37-RESEARCH / 37-PATTERNS).
// ----------------------------------------------------------------------
template <class PMF>
inline void * pmfToVoid(PMF pmf)
{
	static_assert(sizeof(PMF) == sizeof(void *), "inflated PMF (multiple/virtual inheritance) -- needs a thunk");
	return std::bit_cast<void *>(pmf);
}

// ----------------------------------------------------------------------
// Crash-fixer thunk (EPA-02 -- the single most important row). Utinni's
// config::loadOverrideConfig typedef is int(__cdecl*)() (zero-arg orchestrator;
// D:/Code/Utinni/UtinniCore/swg/misc/config.cpp). The spec's best-guess
// buffer-loader is the INNER call, NOT the hooked function. This thunk wraps
// the distinctly-named forwarding shim utinni_installConfigFileOverride()
// (ClientMain.h), which forwards to ClientMainNamespace::installConfigFileOverride().
// Do NOT advertise the buffer-loader symbol here (EPA-02 correction).
// ----------------------------------------------------------------------
static int __cdecl utinni_loadOverrideConfig()
{
	utinni_installConfigFileOverride();
	return 0;
}

// ----------------------------------------------------------------------
// Constructor thunks (37-03). You cannot take &Class::Class in C++. Each
// ctor row is a free-function thunk that placement-constructs on the
// caller-supplied `this` and returns it -- matching UtinniCore's ctor
// typedef EXACTLY (D:/Code/Utinni/UtinniCore/swg/ui/command_parser.cpp:29-30:
// `CommandParser*(__thiscall*)(pThis, args...)`).
//
// CALLING CONVENTION (the landmine): MSVC v145 forbids __thiscall on a free
// function (C3865). The ABI-correct emulation of __thiscall(pThis, a, b, ...)
// is __fastcall(pThis /*ECX*/, dummy /*EDX*/, a, b, ...): __thiscall passes
// `this` in ECX and pushes the rest; __fastcall passes arg1 in ECX, arg2 in
// EDX, and the rest on the stack -- so a dummy EDX param makes the two ABIs
// byte-identical. Utinni's existing __thiscall typedef therefore calls this
// thunk correctly with NO Utinni-side change. (A plain __cdecl/__stdcall thunk
// would mismatch -- this/ECX vs stack -- and crash at the first detour.)
//
// CommandParser is a standalone single-inheritance class (no bases) so the
// `this` pointer needs no most-derived adjustment.
//
// MI-class UI ctors (chatWindow/loginScreen/gameMenu) are DEFERRED -- see the
// OMIT/DEFER block below and 37-03-SUMMARY (same MI-inflation rationale that
// deferred them in 37-02; they also require a live UIPage& arg).
// ----------------------------------------------------------------------
static CommandParser * __fastcall utinni_commandParserCtor1(CommandParser * pThis, int /*edx*/,
	const char * command, size_t argCount, const char * args, const char * helpInfo, CommandParser * delegate)
{
	return ::new (static_cast<void *>(pThis)) CommandParser(command, argCount, args, helpInfo, delegate);
}

static CommandParser * __fastcall utinni_commandParserCtor2(CommandParser * pThis, int /*edx*/,
	const CommandParser::CmdInfo & commandData, CommandParser * delegate)
{
	return ::new (static_cast<void *>(pThis)) CommandParser(commandData, delegate);
}

// ----------------------------------------------------------------------
// consoleHelper::sendInput thunk (WR-05 fix; the cross-repo follow-up for the
// Utinni Phase-24 D-02 carve-out -- the ONE .inc name they left unbound, on RVA,
// pending this provider). The retail target SwgCuiConsoleHelper::sendInput (spec
// line 283; SWGEmu 0x009141D0) has NO from-source twin in this client; the
// engine-layer equivalent is CuiConsoleHelper::processInput. But processInput
// takes a REQUIRED 2nd arg -- a stdset<Unicode::String>& recursion-guard stack
// [CuiConsoleHelper.h:76] -- that an injector calling a one-string
// "sendInput(text)" cannot know to supply, so a raw &CuiConsoleHelper::processInput
// is detoured with garbage/absent arg2 and faults at the first call (WR-05). This
// thunk presents the one-string __thiscall ABI and supplies the recursion stack
// itself from the public static getRecurseStackForCommandBeingParsed()
// [CuiConsoleHelper.h:123 / CuiConsoleHelper.cpp:1136] -- the SAME canonical call
// form the engine uses internally (CuiConsoleHelper.cpp:99). (processInputLine,
// the no-stack sibling, is PRIVATE [:132] so its address cannot be taken here.)
//
// CALLING CONVENTION: same __fastcall(pThis /*ECX*/, dummy /*EDX*/, args) ==
// __thiscall emulation as the ctor thunks above (MSVC v145 forbids __thiscall on
// a free function, C3865). CuiConsoleHelper is single-inheritance (UIEventCallback
// only) so `this` needs no most-derived adjustment.
// UTINNI-SIDE TYPEDEF TO MATCH: bool(__thiscall*)(CuiConsoleHelper*, const Unicode::String&).
// ----------------------------------------------------------------------
static bool __fastcall utinni_consoleHelperSendInput(CuiConsoleHelper * pThis, int /*edx*/,
	const Unicode::String & istr)
{
	return pThis->processInput(istr, CuiConsoleHelper::getRecurseStackForCommandBeingParsed());
}

// ----------------------------------------------------------------------
// groundScene::* MI thunks (38-01, EPA-05). GroundScene is MULTIPLE-INHERITANCE
// (NetworkScene : public Scene, public MessageDispatch::Receiver) so every PMF
// is inflated and trips the pmfToVoid sizeof guard (C2338) -- a __fastcall thunk
// is mandatory; pmfToVoid(&GroundScene::member) must NEVER be used here.
//
// CALLING CONVENTION: same __fastcall(pThis /*ECX*/, int /*EDX*/, args) ==
// __thiscall emulation as the ctor thunks above (MSVC v145 forbids __thiscall on
// a free function, C3865; the dummy EDX makes the two ABIs byte-identical).
//
// These three target PUBLIC GroundScene methods, so they can be named directly
// here (the four PRIVATE methods are reached via the GroundScene.cpp forwarders
// declared in utinni_groundScene_forward.h -- a free thunk in this TU cannot
// name a private member, C2248).
// ----------------------------------------------------------------------
static void __fastcall utinni_groundSceneReloadTerrain(GroundScene * pThis, int /*edx*/)
{
	pThis->reloadTerrain();                                          // public [GroundScene.h:215]
}

static void __fastcall utinni_groundSceneChangeCamera(GroundScene * pThis, int /*edx*/,
	int newView, float value)
{
	pThis->setView(newView, value);                                 // public [GroundScene.h:207] (contract name changeCamera -> ours setView)
}

static GameCamera * __fastcall utinni_groundSceneGetCurrentCamera(GroundScene * pThis, int /*edx*/)
{
	return pThis->getCurrentCamera();                               // public, non-const this -> non-const overload, no cast [GroundScene.h:212]
}

// groundScene::ctor -- placement-new MI ctor thunk (you cannot take &Class::Class;
// 37-03 ctor-thunk pattern). MI class -> the caller-supplied `this` is the
// most-derived pointer; ::new(pThis) constructs the full object via the
// (const char*, const char*, CreatureObject*) overload [GroundScene.h:199] -- NOT
// the (const char*, const NetworkId&, ...) overload at :200.
static GroundScene * __fastcall utinni_groundSceneCtor(GroundScene * pThis, int /*edx*/,
	const char * terrainFilename, const char * playerFilename, CreatureObject * customizedPlayer)
{
	return ::new (static_cast<void *>(pThis)) GroundScene(terrainFilename, playerFilename, customizedPlayer);
}

// ----------------------------------------------------------------------
// cui::chatWindow::* MI thunks (38-03, EPA-05 remainder). SwgCuiChatWindow is
// TRIPLE-INHERITANCE (public SwgCuiLockableMediator, public UINotification,
// public MessageDispatch::Receiver [SwgCuiChatWindow.h:58-61]) so every PMF is
// inflated and trips the pmfToVoid sizeof guard (C2338) -- a __fastcall thunk is
// mandatory; pmfToVoid(&SwgCuiChatWindow::member) must NEVER be used here.
//
// CALLING CONVENTION: same __fastcall(pThis /*ECX*/, int /*EDX*/, args) ==
// __thiscall emulation as the groundScene/ctor thunks above (MSVC v145 forbids
// __thiscall on a free function, C3865; the dummy EDX makes the two ABIs
// byte-identical). All four targets are PUBLIC non-virtual methods [confirmed
// SwgCuiChatWindow.h public block :63-231], so they are named directly here --
// no friend decl needed (unlike the 38-01 groundScene private forwarders /
// 38-02 Os::WindowProc shim, which targeted private members).
//
// The contract names are the SPEC names (enableTextInput/writeToAllTabs/
// writeToCurrentTab/chatEnterHandler); the in-tree methods have DIFFERENT names
// (acceptTextInput/appendToAllTabs/appendTextToCurrentTab/performEnterKey) --
// the NAME MISMATCH is baked into each row comment, the contract names stay.
// ----------------------------------------------------------------------
static void __fastcall utinni_chatWindowAcceptTextInput(SwgCuiChatWindow * pThis, int /*edx*/,
	bool b, bool setKeyboardInput, bool unfocusMediator)
{
	pThis->acceptTextInput(b, setKeyboardInput, unfocusMediator);   // public [SwgCuiChatWindow.h:112] (contract enableTextInput -> ours acceptTextInput)
}

static void __fastcall utinni_chatWindowAppendToAllTabs(SwgCuiChatWindow * pThis, int /*edx*/,
	const Unicode::String & str)
{
	pThis->appendToAllTabs(str);                                    // public [SwgCuiChatWindow.h:172] (contract writeToAllTabs -> ours appendToAllTabs)
}

static void __fastcall utinni_chatWindowAppendTextToCurrentTab(SwgCuiChatWindow * pThis, int /*edx*/,
	const Unicode::String & str)
{
	pThis->appendTextToCurrentTab(str);                             // public [SwgCuiChatWindow.h:174] (contract writeToCurrentTab -> ours appendTextToCurrentTab)
}

// chatEnterHandler -> the CLEAN function-entry performEnterKey() [SwgCuiChatWindow.h:214],
// public non-virtual. This advertises the clean &fn entry ONLY. The Issue #11
// mid-function chat-context-routing NOP that Utinni patches mid-body on SWGEmu is
// a SEPARATE, SWGEmu-only joint decision (38-CONTEXT.md OUT-OF-SCOPE: "Mid-function-
// patch features (Issue #11 chat routing)") -- NOT implemented here; no offset
// arithmetic enters the contract. Flagged for the EPA-08 handback.
static void __fastcall utinni_chatWindowPerformEnterKey(SwgCuiChatWindow * pThis, int /*edx*/)
{
	pThis->performEnterKey();                                       // public clean entry [SwgCuiChatWindow.h:214] (contract chatEnterHandler; Issue #11 mid-patch is a separate joint decision, NOT here)
}

// ----------------------------------------------------------------------
// The advertised table. CANONICAL FORM (pinned 2026-06-21): NO null-pair
// sentinel terminator row; count = sizeof/sizeof (NO -1). 37-02/03 MUST NOT
// reintroduce a sentinel. Per-row symbol kind is noted in the comment.
// ----------------------------------------------------------------------
static const UtinniEngineHookPoint s_engineHookPoints[] =
{
	// -- config (sharedFoundation, all static; ConfigFile.h) -------------------
	{ "config::loadOverrideConfig",   (void *)&utinni_loadOverrideConfig },        // EPA-02 crash-fixer thunk (installConfigFileOverride, not the buffer-loader)
	{ "config::loadConfigFileBuffer", (void *)&ConfigFile::loadFromBuffer },       // static bool loadFromBuffer(char const*,int) [ConfigFile.h:136]
	{ "config::loadConfigFileString", (void *)&ConfigFile::loadFile },             // static bool loadFile(char const*) [ConfigFile.h:135] (MISMATCH: spec "loadFromString" -> ours is loadFile)
	{ "config::setModalChat",         (void *)&CuiPreferences::setModalChat },     // 38-02 (37-02 CORRECTION): plain &fn -- a PUBLIC CuiPreferences static, NOT config/ConfigFile [CuiPreferences.h:95, def CuiPreferences.cpp:1267]; contract name stays config::setModalChat
	{ "config::getModalChat",         (void *)&CuiPreferences::getModalChat },     // 38-02 (37-02 CORRECTION): plain &fn -- a PUBLIC CuiPreferences static, NOT config/ConfigFile [CuiPreferences.h:94, def CuiPreferences.cpp:1655]

	// -- client (exe entry + win32 exe-statics; ClientMain.h / Os.cpp / DebugHelp.cpp) --
	{ "client::clientMain",           (void *)&ClientMain },                       // int ClientMain(HINSTANCE,HINSTANCE,LPSTR,int) __cdecl [ClientMain.h:13]
	{ "client::wndProc",              (void *)&utinni_osWindowProc },              // 38-02: external __stdcall/CALLBACK shim in Os.cpp over the PRIVATE Os::WindowProc [Os.h:138] (friend-granted member access); CALLBACK preserved
	{ "client::writeMiniDump",        (void *)&utinni_writeMiniDump },             // 38-02: external shim in DebugHelp.cpp over DebugHelp::writeMiniDump [DebugHelp.h:36] (win32-private header not on the exe include path)
	// OMIT (38-02, D-04 / Pitfall 5): client::writeCrashLog + client::setupStartDataInstall are NONEXISTENT in this tree (grep = 0 source hits). The crash .txt is written INLINE by SetupSharedFoundation's exception handler [SetupSharedFoundation.cpp:92 sprintf] -- no named writeCrashLog function; setupStartDataInstall is a SWGEmu Pre-CU concept with no from-source twin. NOT advertised (never guessed); FLAGGED for the EPA-08 handback.

	// -- game (clientGame, all static; Game.h) ---------------------------------
	{ "game::install",                (void *)&Game::install },                    // static void install(Application) [Game.h:94]
	{ "game::quit",                   (void *)&Game::quit },                       // static void quit() [Game.h:98]
	{ "game::mainLoop",               (void *)&Game::run },                        // static void run() [Game.h:96] (MISMATCH: spec "runGame" -> ours run)
	{ "game::setupScene",             (void *)static_cast<void(*)(bool, const char *, const char *, CreatureObject *)>(&Game::setScene) }, // OVERLOADED [Game.h:108] -> cast to the (terrain,player,customized) overload
	{ "game::cleanupScene",           (void *)&Game::cleanupScene },               // static void cleanupScene() [Game.h:101]
	{ "game::getPlayer",              (void *)&Game::getPlayer },                  // static Object* getPlayer() [Game.h:150]
	{ "game::getPlayerCreatureObject",(void *)&Game::getPlayerCreature },          // static CreatureObject* getPlayerCreature() [Game.h:157] (MISMATCH name)
	{ "game::getCamera",              (void *)&Game::getCamera },                  // static Camera* getCamera() [Game.h:173] -- NOT overloaded (const sibling is getConstCamera); NO cast
	{ "game::getConstCamera",         (void *)&Game::getConstCamera },             // static Camera const* getConstCamera() [Game.h:174]
	{ "game::isViewFirstPerson",      (void *)&Game::isViewFirstPerson },          // static bool isViewFirstPerson() [Game.h:185]
	{ "game::isHudSceneTypeSpace",    (void *)&Game::isHudSceneTypeSpace },        // static bool isHudSceneTypeSpace() [Game.h:215]
	{ "game::g_runningFlags",         (void *)&Game::isOver },                     // ACCESSOR (sec 8 #3): ms_done is private; isOver() is the NON-inline accessor [Game.h:134 / Game.cpp:1021] -- call-not-read

	// -- graphics (clientGraphics facade, all static; Graphics.h) [EPA-03] -----
	{ "graphics::install",            (void *)&Graphics::install },                // static bool install() [Graphics.h:70] -- EPA-03 DX11 overlay kickoff row
	{ "graphics::update",             (void *)&Graphics::update },                 // static void update(float) [Graphics.h:153]
	{ "graphics::beginScene",         (void *)&Graphics::beginScene },             // static void beginScene() [Graphics.h:155]
	{ "graphics::endScene",           (void *)&Graphics::endScene },               // static void endScene() [Graphics.h:156]
	{ "graphics::present",            (void *)static_cast<bool(*)()>(&Graphics::present) },              // OVERLOADED [Graphics.h:161] -> no-arg present()
	{ "graphics::presentWindow",      (void *)static_cast<bool(*)(HWND, int, int)>(&Graphics::present) },// OVERLOADED [Graphics.h:162] -> present(HWND,int,int)
	{ "graphics::resize",             (void *)&Graphics::resize },                 // static void resize(int,int) [Graphics.h:130]
	{ "graphics::flushResources",     (void *)&Graphics::flushResources },         // static void flushResources(bool) [Graphics.h:81]
	{ "graphics::screenshot",         (void *)&Graphics::screenShot },             // static bool screenShot(const char*) [Graphics.h:170] (MISMATCH name)
	{ "graphics::useHardwareCursor",  (void *)&Graphics::setHardwareMouseCursorEnabled }, // static void setHardwareMouseCursorEnabled(bool) [Graphics.h:177] (MISMATCH name)
	{ "graphics::showMouseCursor",    (void *)&Graphics::showMouseCursor },        // static bool showMouseCursor(bool) [Graphics.h:180]
	{ "graphics::setSystemMouseCursorPosition", (void *)&Graphics::setSystemMouseCursorPosition }, // static void(int,int) [Graphics.h:181]
	{ "graphics::setStaticShader",    (void *)&Graphics::setStaticShader },        // static void setStaticShader(const StaticShader&,int) [Graphics.h:175]
	{ "graphics::g_renderTargetWidth",  (void *)&Graphics::getCurrentRenderTargetWidth },  // ACCESSOR (sec 8 #3): RT width behind a static getter [Graphics.h:103] -- call-not-read
	{ "graphics::g_renderTargetHeight", (void *)&Graphics::getCurrentRenderTargetHeight }, // ACCESSOR (sec 8 #3): RT height behind a static getter [Graphics.h:104] -- call-not-read

	// -- scene::groundScene (clientGame; GroundScene.h) -- 38-01, EPA-05 ---------
	// GroundScene is MULTIPLE-INHERITANCE (NetworkScene : public Scene, public
	// MessageDispatch::Receiver [NetworkScene.h:28-30]) -> every PMF is inflated,
	// so NONE of these use pmfToVoid(&GroundScene::member) (would trip the C2338
	// sizeof guard). 3 PUBLIC methods + 1 ctor are __fastcall thunks defined above;
	// the 4 PRIVATE methods are __fastcall forwarders defined in GroundScene.cpp
	// (member access) and declared in utinni_groundScene_forward.h.
	{ "groundScene::ctor",                 (void *)&utinni_groundSceneCtor },                    // placement-new MI ctor thunk -> GroundScene(const char*,const char*,CreatureObject*) [GroundScene.h:199] (NOT the NetworkId overload :200)
	{ "groundScene::init",                 (void *)&utinni_groundSceneInit },                    // PRIVATE [GroundScene.h:173] -> in-TU GroundScene.cpp forwarder (member access)
	{ "groundScene::reloadTerrain",        (void *)&utinni_groundSceneReloadTerrain },           // public [GroundScene.h:215] -> MI __fastcall thunk
	{ "groundScene::changeCamera",         (void *)&utinni_groundSceneChangeCamera },            // public [GroundScene.h:207] MISMATCH: ours setView(int,float) -> MI __fastcall thunk
	{ "groundScene::getCurrentCamera",     (void *)&utinni_groundSceneGetCurrentCamera },        // public [GroundScene.h:212] (non-const overload; const sibling :213) -> MI __fastcall thunk
	{ "groundScene::update",               (void *)&utinni_groundSceneUpdate },                  // PRIVATE [GroundScene.h:77] -> in-TU GroundScene.cpp forwarder
	{ "groundScene::handleInputMapUpdate", (void *)&utinni_groundSceneHandleInputMapUpdate },    // PRIVATE [GroundScene.h:170] -> in-TU GroundScene.cpp forwarder
	{ "groundScene::handleInputMapEvent",  (void *)&utinni_groundSceneHandleInputMapEvent },     // PRIVATE [GroundScene.h:168] -> in-TU GroundScene.cpp forwarder
	// SKIP: groundScene::draw -- VIRTUAL [GroundScene.h:204] (also in the VIRTUAL SKIPS block); Utinni resolves off the live vtable. Not advertised.
	// OMIT: groundScene::g_instance -- no dedicated GroundScene singleton; reached via INLINE Game::getScene() [Game.h:306] (no ODR address) cast to GroundScene, and Game::ms_scene is private [Game.h:271]. OMITTED (graceful degradation); FLAGGED for the EPA-08 handback -- if Utinni's groundScene editor strictly needs the raw singleton pointer, add a non-inline Game accessor in a follow-up.

	// -- cui::chatWindow (swgClientUserInterface; SwgCuiChatWindow.h) -- 38-03, EPA-05 --
	// SwgCuiChatWindow is TRIPLE-INHERITANCE (public SwgCuiLockableMediator, public
	// UINotification, public MessageDispatch::Receiver [SwgCuiChatWindow.h:58-61]) ->
	// every PMF is inflated, so NONE of these use pmfToVoid(&SwgCuiChatWindow::member)
	// (would trip the C2338 sizeof guard). All 4 target PUBLIC non-virtual methods, so
	// they are reached directly by the __fastcall thunks defined above (no friend decl).
	// Contract names are the SPEC names; the in-tree method NAME MISMATCH is in each comment.
	{ "cuiChatWindow::enableTextInput",   (void *)&utinni_chatWindowAcceptTextInput },         // public [SwgCuiChatWindow.h:112] MISMATCH: ours acceptTextInput(bool,bool,bool) -> MI __fastcall thunk
	{ "cuiChatWindow::writeToAllTabs",    (void *)&utinni_chatWindowAppendToAllTabs },         // public [SwgCuiChatWindow.h:172] MISMATCH: ours appendToAllTabs(const Unicode::String&) -> MI __fastcall thunk
	{ "cuiChatWindow::writeToCurrentTab", (void *)&utinni_chatWindowAppendTextToCurrentTab },  // public [SwgCuiChatWindow.h:174] MISMATCH: ours appendTextToCurrentTab(const Unicode::String&) -> MI __fastcall thunk
	{ "cuiChatWindow::chatEnterHandler",  (void *)&utinni_chatWindowPerformEnterKey },         // public CLEAN ENTRY [SwgCuiChatWindow.h:214] MISMATCH: ours performEnterKey() -> MI __fastcall thunk. Issue #11 mid-function chat-routing NOP is a SEPARATE SWGEmu-only joint decision (38-CONTEXT.md OUT-OF-SCOPE), NOT implemented; no offset arithmetic in the contract -- flagged for EPA-08.
	// DEFER: cuiChatWindow::ctor -- SwgCuiChatWindow(UIPage&, Game::SceneType, std::string const&) [SwgCuiChatWindow.h:106] is an MI ctor requiring a LIVE UIPage& arg the injector must supply (37-03 already deferred this exact ctor; matches groundScene/UI-mediator MI-ctor DEFER rationale). NOT advertised (no ctor row, no .inc name); FLAGGED for the EPA-08 handback -- if Utinni confirms it constructs chat windows through a page, add a placement-new __fastcall thunk in a follow-up.

	// -- cui::manager (clientUserInterface, all static; CuiManager.h) -----------
	{ "cuiManager::render",           (void *)&CuiManager::render },               // static void render() [CuiManager.h:88]
	{ "cuiManager::setSize",          (void *)&CuiManager::setSize },              // static void setSize(int,int) [CuiManager.h:129]
	{ "cuiManager::togglePointer",    (void *)&CuiManager::setPointerToggledOn },  // static void setPointerToggledOn(bool) [CuiManager.h:107] (MISMATCH name)
	{ "cuiManager::restartMusic",     (void *)&CuiManager::restartMusic },         // static void restartMusic(bool) [CuiManager.h:97]
	{ "cuiManager::g_instance",       (void *)&CuiManager::getIoWin },             // ACCESSOR: CuiManager is all-static (no instance); getIoWin() returns the CuiIoWin singleton [CuiManager.h:100]

	// -- cui::io (clientUserInterface; CuiIoWin.h) ------------------------------
	// NOTE: cui::io::processEvent (CuiIoWin.h:62) and ::draw (:61) are VIRTUAL overrides of
	// IoWin -- SKIP: Utinni resolves off the live vtable (spec sec 6). Not advertised as &fn
	// (a &CuiIoWin::processEvent would be a vtable-dispatch thunk, not the impl).
	{ "cuiIo::setKeyboardInputActive",pmfToVoid(&CuiIoWin::setKeyboardInputActive) }, // member PMF __thiscall, single-inheritance [CuiIoWin.h:66]
	{ "cuiIo::requestKeyboard",       pmfToVoid(&CuiIoWin::requestKeyboard) },     // member PMF __thiscall [CuiIoWin.h:102]
	{ "cuiIo::g_instance",            (void *)&CuiManager::getIoWin },             // ACCESSOR: the CuiIoWin singleton accessor (CuiManager owns ms_theIoWin) [CuiManager.h:100]

	// -- consoleHelper (clientUserInterface; CuiConsoleHelper.h) ----------------
	{ "consoleHelper::sendInput",     (void *)&utinni_consoleHelperSendInput },     // WR-05 fix: __fastcall(pThis,edx,istr)==__thiscall thunk -> processInput(istr, getRecurseStackForCommandBeingParsed()) [CuiConsoleHelper.h:76]; a raw &processInput would fault on the missing recursion-stack arg2 (spec "sendInput" -> SwgCuiConsoleHelper::sendInput has no from-source twin; engine-layer processInput is the equivalent)

	// -- commandParser (sharedCommandParser; CommandParser.h) -------------------
	{ "commandParser::addSubCommand", pmfToVoid(&CommandParser::addSubCommand) },  // bit_cast member PMF, __thiscall [CommandParser.h:149]

	// ======================================================================
	// 37-03 FULL CATALOG. Per-row symbol kind in the comment. Every & resolved
	// against the cited header this wave. Symbols the plan named but that do
	// NOT exist / are virtual / inline are OMITTED (see the OMIT block below).
	// ======================================================================

	// -- extent (sharedCollision; BaseExtent.h) §8 #2 --------------------------
	// The NON-virtual BaseExtent::intersect(begin,end) const overload via PMF +
	// explicit overload static_cast. Single-inheritance base -> PMF not inflated.
	// Collapses the UtinniCore retail(0x0126AF70)/SWGEmu(0x0125FA10) RVA split.
	{ "extent::intersect",            pmfToVoid(static_cast<bool (BaseExtent::*)(Vector const &, Vector const &) const>(&BaseExtent::intersect)) }, // OVERLOADED non-virtual [BaseExtent.h:47]

	// -- object (sharedObject; Object.h) -- NON-VIRTUAL only -------------------
	// VIRTUAL skips (NOT advertised; Utinni resolves off the live vtable, spec §6):
	//   Object::addToWorld / removeFromWorld [Object.h:120-121], setParentCell [:165].
	// move_o is INLINE [Object.h:1216] -> OMITTED (no ODR-emitted address, Pitfall 2).
	{ "object::getObjectType",        pmfToVoid(&Object::getObjectType) },         // Tag getObjectType() const NON-virtual [Object.h:152]
	{ "object::getObjectTemplate",    pmfToVoid(&Object::getObjectTemplate) },     // const ObjectTemplate* getObjectTemplate() const [Object.h:150]
	{ "object::getObjectTemplateName",pmfToVoid(&Object::getObjectTemplateName) }, // const char* getObjectTemplateName() const [Object.h:151]
	{ "object::getNetworkId",         pmfToVoid(&Object::getNetworkId) },          // const NetworkId& getNetworkId() const [Object.h:162]
	{ "object::getParentCell",        pmfToVoid(&Object::getParentCell) },         // CellProperty* getParentCell() const NON-virtual [Object.h:166]
	{ "object::getTransform_o2w",     pmfToVoid(&Object::getTransform_o2w) },      // Transform const& getTransform_o2w() const DLLEXPORT [Object.h:243]
	{ "object::setTransform_o2w",     pmfToVoid(&Object::setTransform_o2w) },      // void setTransform_o2w(const Transform&) [Object.h:248]
	{ "object::getPosition_w",        pmfToVoid(&Object::getPosition_w) },         // const Vector getPosition_w() const [Object.h:245]
	{ "object::setPosition_w",        pmfToVoid(&Object::setPosition_w) },         // void setPosition_w(const Vector&) [Object.h:247]
	{ "object::getAppearance",        pmfToVoid(static_cast<Appearance * (Object::*)()>(&Object::getAppearance)) }, // OVERLOADED (const/non-const) [Object.h:170-171]
	{ "object::setAppearance",        pmfToVoid(&Object::setAppearance) },         // void setAppearance(Appearance*) [Object.h:174]
	{ "object::move_p",               pmfToVoid(&Object::move_p) },                // void move_p(const Vector&) NON-virtual non-inline [Object.h:251]

	// -- objectTemplate (sharedObject base static + sharedGame SharedObjectTemplate) --
	{ "objectTemplate::createObject", (void *)static_cast<Object * (*)(const char *)>(&ObjectTemplate::createObject) }, // OVERLOADED: static createObject(const char*) [ObjectTemplate.h:32] vs virtual createObject() const [:50]
	{ "objectTemplate::getAppearanceFilename",   pmfToVoid(&SharedObjectTemplate::getAppearanceFilename) },   // const std::string& (bool=false) const NON-virtual [SharedObjectTemplate.h:353]
	{ "objectTemplate::getPortalLayoutFilename", pmfToVoid(&SharedObjectTemplate::getPortalLayoutFilename) }, // [SharedObjectTemplate.h:354]
	{ "objectTemplate::getClientDataFile",       pmfToVoid(&SharedObjectTemplate::getClientDataFile) },       // [SharedObjectTemplate.h:355]

	// -- worldSnapshot (clientGame; WorldSnapshot.h) -- ALL STATIC in this tree -
	{ "worldSnapshot::load",          (void *)&WorldSnapshot::load },              // static void load(char const*) [WorldSnapshot.h:44]
	{ "worldSnapshot::addObject",     (void *)&WorldSnapshot::addObject },         // static Object* addObject(...) [WorldSnapshot.h:33]
	{ "worldSnapshot::removeObject",  (void *)&WorldSnapshot::removeObject },      // static void removeObject(int64) [WorldSnapshot.h:49]
	{ "worldSnapshot::moveObject",    (void *)&WorldSnapshot::moveObject },        // static void moveObject(int64,Transform const&) [WorldSnapshot.h:48]
	{ "worldSnapshot::getLoadingPercent",   (void *)&WorldSnapshot::getLoadingPercent },   // static int getLoadingPercent() [WorldSnapshot.h:52]
	{ "worldSnapshot::detailLevelChanged",  (void *)&WorldSnapshot::detailLevelChanged },  // static void detailLevelChanged() [WorldSnapshot.h:56]

	// -- camera (clientGraphics; Camera.h) -- NON-VIRTUAL non-inline setters ----
	// (getViewport*/getViewportWidth etc. are INLINE [Camera.h:210-258] -> OMITTED.)
	{ "camera::setViewport",          pmfToVoid(static_cast<void (Camera::*)(int, int, int, int)>(&Camera::setViewport)) }, // OVERLOADED [Camera.h:172]
	{ "camera::setNearPlane",         pmfToVoid(&Camera::setNearPlane) },          // void setNearPlane(real) [Camera.h:174]
	{ "camera::setFarPlane",          pmfToVoid(&Camera::setFarPlane) },           // void setFarPlane(real) [Camera.h:175]
	{ "camera::setHorizontalFieldOfView", pmfToVoid(&Camera::setHorizontalFieldOfView) }, // void(real) [Camera.h:178]
	{ "camera::reverseProjectInViewportSpace", pmfToVoid(static_cast<const Vector (Camera::*)(int, int) const>(&Camera::reverseProjectInViewportSpace)) }, // OVERLOADED non-inline [Camera.h:160]

	// -- misc statics (memory/audio/file/report) -------------------------------
	{ "memory::allocate",             (void *)&MemoryManager::allocate },          // static void* allocate(size_t,uint32,bool,bool) DLLEXPORT [MemoryManager.h:58]
	{ "memory::free",                 (void *)&MemoryManager::free },              // static void free(void*,bool) DLLEXPORT [MemoryManager.h:59] (spec "deallocate" -> ours free)
	{ "audio::setMasterVolume",       (void *)&Audio::setMasterVolume },           // static void setMasterVolume(float) [Audio.h:165]
	{ "audio::getMasterVolume",       (void *)&Audio::getMasterVolume },           // static float getMasterVolume() [Audio.h:166]
	{ "treeFile::open",               (void *)&TreeFile::open },                   // static AbstractFile* open(const char*,PriorityType,bool) DLLEXPORT [TreeFile.h:85]
	{ "report::print",                (void *)&Report::puts },                     // static void puts(const char*) [Report.h:41] (spec "Report::print" -> ours puts; printf is variadic)

	// -- commandParser ctors (free-function __thiscall thunks; NEVER &Class::Class) --
	{ "commandParser::ctor1",         (void *)&utinni_commandParserCtor1 },        // __fastcall(pThis,edx,..)==__thiscall thunk -> CommandParser(const char*,size_t,const char*,const char*,CommandParser*) [CommandParser.h:130] (matches UtinniCore pCtor1)
	{ "commandParser::ctor2",         (void *)&utinni_commandParserCtor2 },        // __fastcall(pThis,edx,..)==__thiscall thunk -> CommandParser(const CmdInfo&,CommandParser*) [CommandParser.h:128] (matches UtinniCore pCtor2)

	// ======================================================================
	// VIRTUAL SKIPS (37-03) -- advertised as NOTHING. &fn on a virtual yields a
	// vtable-dispatch thunk, not the impl; Utinni resolves these off the live
	// vtable (spec §6). They are intentionally NOT in the .inc required set
	// (a skipped virtual is vtable-resolved, NOT a "missing" required row).
	//   SKIP: virtual -- Object::addToWorld          [Object.h:120]
	//   SKIP: virtual -- Object::removeFromWorld      [Object.h:121]
	//   SKIP: virtual -- Object::setParentCell        [Object.h:165]
	//   SKIP: virtual -- Appearance::render           (Appearance.h, pure/virtual)
	//   SKIP: virtual -- Appearance::collide          (Appearance.h, virtual)
	//   SKIP: virtual -- GroundScene::draw            (Scene/IoWin override; deferred from 37-02)
	//   SKIP: virtual -- RenderWorld::render          (RenderWorld.h, static-or-virtual; vtable path)
	//   SKIP: virtual -- CuiIoWin::processEvent       [CuiIoWin.h:62] (from 37-02)
	//   SKIP: virtual -- BaseExtent::realIntersect    [BaseExtent.h:69] protected-virtual (the §8 #2 non-virtual BaseExtent::intersect IS advertised above)
	// (getObjectType/move_p/getParentCell are NON-virtual -> advertised in Task 1, NOT skipped.)

	// -- globals (§8 #3): advertised via accessor (call-not-read) where the data
	//    is private/file-static; raw &g only where genuinely accessible. ---------
	// The MVP already advertises the canonical accessors (game::g_runningFlags ->
	// Game::isOver; graphics RT W/H -> getCurrentRenderTarget*; cui singletons ->
	// CuiManager::getIoWin). The remaining spec globals (player health/stats, hud
	// view-distance, terrain singleton/weather/filename, static-shader) are
	// private members with NO non-inline ODR-emitted accessor confirmable this
	// wave -> OMITTED (see OMIT/DEFER block + 37-03-SUMMARY). A genuinely
	// accessible engine singleton accessor advertised raw as &g:
	{ "graphics::g_frameNumber",     (void *)&Graphics::getFrameNumber },          // ACCESSOR (§8 #3): the frame counter behind a genuinely-accessible non-private static getter [Graphics.h:83] -- call-not-read, &g-style global row
	// PINNED: NO null-pair sentinel -- count is sizeof/sizeof, the static_assert has NO -1.
};

static const UtinniEngineHookPoints s_table =
{
	UTINNI_HOOKPOINTS_VERSION,
	(unsigned int)(sizeof s_engineHookPoints / sizeof s_engineHookPoints[0]),   // NO -1 (no sentinel)
	s_engineHookPoints
};

// ----------------------------------------------------------------------
// Coverage self-check (EPA-04 seed -- 37-02/37-03 inherit it). THREE parts.
// The count static_assert is only a cheap drift SMOKE; the name-set-equality
// runtime check is the actual zero-missing gate.
// ----------------------------------------------------------------------

// (a) Compile-time count smoke: expand the .inc to a +1 count and assert the
// table row count equals the .inc required-set count. NO -1 (no sentinel).
// Mirrors the Direct3d11_ConstantBuffer.h static_assert table-validation idiom.
enum
{
	UTINNI_REQUIRED_COUNT = 0
#define UTINNI_HOOKPOINT(g, n) + 1
#include "utinni_engine_hookpoints.inc"
#undef UTINNI_HOOKPOINT
};
static_assert((sizeof s_engineHookPoints / sizeof s_engineHookPoints[0]) == UTINNI_REQUIRED_COUNT,
              "hookpoint table row count != .inc required-set count (drift)");

// (b) Name-set source-of-truth: the machine-generated required-name set,
// emitted from the SAME .inc via the X-macro -- no hand-typed name strings.
static const char * const s_requiredNames[] =
{
#define UTINNI_HOOKPOINT(g, n) #g "::" #n,
#include "utinni_engine_hookpoints.inc"
#undef UTINNI_HOOKPOINT
};

// (c) Runtime self-check. Never crashes (graceful degradation, EPA-04):
// returns false / logs on ANY of: a null addr, a duplicate name, OR a name-set
// mismatch vs s_requiredNames[] (every required name present exactly once and
// no extras -- the zero-missing check the count static_assert cannot provide).
static bool utinni_strEq(const char * a, const char * b)
{
	if (a == b)
		return true;
	if (!a || !b)
		return false;
	while (*a && (*a == *b))
	{
		++a;
		++b;
	}
	return *a == *b;
}

bool utinni_verifyNoNullNoDup()
{
	bool ok = true;
	const unsigned int count = s_table.count;

	// (a) no null addr
	for (unsigned int i = 0; i < count; ++i)
	{
		if (s_engineHookPoints[i].addr == 0)
		{
			REPORT_LOG(true, ("utinni_verifyNoNullNoDup: NULL addr for '%s'\n",
				s_engineHookPoints[i].name ? s_engineHookPoints[i].name : "(null)"));
			ok = false;
		}
	}

	// (b) no duplicate name
	for (unsigned int i = 0; i < count; ++i)
	{
		for (unsigned int j = i + 1; j < count; ++j)
		{
			if (utinni_strEq(s_engineHookPoints[i].name, s_engineHookPoints[j].name))
			{
				REPORT_LOG(true, ("utinni_verifyNoNullNoDup: DUPLICATE name '%s'\n",
					s_engineHookPoints[i].name ? s_engineHookPoints[i].name : "(null)"));
				ok = false;
			}
		}
	}

	// (c) name-set equality vs the X-macro-generated required set:
	//     every required name appears in the table exactly once...
	const unsigned int requiredCount = (unsigned int)(sizeof s_requiredNames / sizeof s_requiredNames[0]);
	for (unsigned int r = 0; r < requiredCount; ++r)
	{
		unsigned int hits = 0;
		for (unsigned int i = 0; i < count; ++i)
		{
			if (utinni_strEq(s_requiredNames[r], s_engineHookPoints[i].name))
				++hits;
		}
		if (hits != 1)
		{
			REPORT_LOG(true, ("utinni_verifyNoNullNoDup: required name '%s' present %u times (expected 1)\n",
				s_requiredNames[r], hits));
			ok = false;
		}
	}

	//     ...and the table adds no extras outside the required set.
	for (unsigned int i = 0; i < count; ++i)
	{
		bool found = false;
		for (unsigned int r = 0; r < requiredCount; ++r)
		{
			if (utinni_strEq(s_engineHookPoints[i].name, s_requiredNames[r]))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			REPORT_LOG(true, ("utinni_verifyNoNullNoDup: table name '%s' not in required set\n",
				s_engineHookPoints[i].name ? s_engineHookPoints[i].name : "(null)"));
			ok = false;
		}
	}

	REPORT_LOG(true, ("utinni_verifyNoNullNoDup: %s (%u rows, %u required)\n",
		ok ? "PASS" : "FAIL", count, requiredCount));
	return ok;
}

// ----------------------------------------------------------------------
// The export. dllexport ALONE on an extern "C" __cdecl function forces the
// undecorated public name -- NO .def, NO /EXPORT pragma, NO ModuleDefinitionFile
// (proven by the shipped gl11 GetHookPoints twin, dumpbin-confirmed). Returns a
// pointer to the process-lifetime static; Utinni only reads it.
// ----------------------------------------------------------------------
extern "C" __declspec(dllexport) const UtinniEngineHookPoints * __cdecl GetEngineHookPoints();

const UtinniEngineHookPoints * GetEngineHookPoints()
{
	return &s_table;
}

#endif // !defined(_WIN64)
