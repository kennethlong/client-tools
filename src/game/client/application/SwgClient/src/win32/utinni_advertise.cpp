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

	// -- client (exe entry; ClientMain.h) --------------------------------------
	{ "client::clientMain",           (void *)&ClientMain },                       // int ClientMain(HINSTANCE,HINSTANCE,LPSTR,int) __cdecl [ClientMain.h:13]

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

	// -- scene::groundScene -- DEFERRED to 37-03 (Rule 3 landmine discovered at build):
	// GroundScene derives (via NetworkScene) from MULTIPLE bases -- NetworkScene.h:28-30
	// `class NetworkScene : public Scene, public MessageDispatch::Receiver`. A
	// multiple-inheritance class has INFLATED member-function pointers (>sizeof(void*)),
	// so &GroundScene::{reloadTerrain,getCurrentCamera,setView} trip the pmfToVoid
	// sizeof(PMF)==sizeof(void*) guard (C2338). Even the "clean" public non-virtual rows
	// need a __thiscall free-function thunk (the MI-class thunk pattern is 37-03's tier).
	// OMITTED here (NOT weakened) -- a wrong/inflated & is worse than a missing row (spec sec 0).
	// Names also removed from the .inc so the coverage gate stays in lockstep.

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
	{ "consoleHelper::sendInput",     pmfToVoid(&CuiConsoleHelper::processInput) },// member PMF __thiscall, single-inheritance [CuiConsoleHelper.h:76] (MISMATCH: spec "sendInput" -> ours processInput)

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
