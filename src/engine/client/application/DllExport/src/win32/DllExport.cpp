// ======================================================================
//
// DllExport.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

// make windows.h more strict in the types of handles
#ifndef STRICT
#define STRICT 1
#endif

// trim down the amount of stuff windows.h includes
#define NOGDICAPMASKS
#define NOVIRTUALKEYCODE
#define NOKEYSTATES
#define NORASTEROPS
#define NOATOM
#define NOCOLOR
#define NODRAWTEXT
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSERVICE
#define NOSOUND
#define NOCOMM
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wtypes.h>

// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"

#include "clientGraphics/DynamicIndexBuffer.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Material.h"
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureFormatInfo.h"
#include "clientGraphics/VertexBufferVector.h"
#include "sharedDebug/DataLint.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/DebugKey.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedDebug/Profiler.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ConfigSharedFoundation.h"
#include "sharedFoundation/CrashReportInformation.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Fatal.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/PersistentCrcString.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedMath/Transform.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Object.h"
#include "sharedSynchronization/Mutex.h"

// ======================================================================
// Phase 33 (X64-02): DllExport is the plugin<->host engine-symbol bridge. Every body here is an
// unreachable EXPORT STUB -- it exists only to emit an import library (DllExport.lib) so the gl05/06/07
// plugins can resolve their engine `dllimport` references at link; at RUNTIME the loader binds those
// imports to the real engine inside SwgClient.exe, so these stub bodies are never executed. The x86
// `__asm int 3` breakpoint trap is x64-illegal; DLLEXPORT_UNREACHABLE_TRAP maps to the portable
// __debugbreak() intrinsic on x64 (identical int-3 semantics) and keeps the byte-identical `__asm int 3`
// on x86. This is a mechanical port of a never-called trap, NOT a behavior change.
#if defined(_M_X64)
#  define DLLEXPORT_UNREACHABLE_TRAP() __debugbreak()
#else
#  define DLLEXPORT_UNREACHABLE_TRAP() __asm int 3
#endif

void Fatal(const char *, ...)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void DebugFatal(const char *, ...)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void Warning(const char *, ...)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

void Report::setFlags(int)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void Report::vprintf(const char *, va_list)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void Report::printf(const char *, ...)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

bool ExitChain::isFataling()
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ======================================================================

bool ConfigSharedFoundation::getVerboseHardwareLogging()
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ======================================================================

Mutex::Mutex()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

Mutex::~Mutex()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

const TextureFormatInfo &TextureFormatInfo::getInfo(TextureFormat)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	static TextureFormatInfo dummy;
	return dummy;
}

void TextureFormatInfo::setSupported(TextureFormat, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

void *MemoryManager::allocate(size_t, uint32, bool, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return NULL;
}

void  MemoryManager::free(void *, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void  MemoryManager::own(void *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

bool DataLint::isEnabled()
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ======================================================================

void DebugFlags::registerFlag(bool &, const char *, const char *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void DebugFlags::registerFlag(bool &, const char *, const char *, ReportRoutine1, int)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void DebugFlags::unregisterFlag(bool &)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

void DebugKey::registerFlag(bool &, const char *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ----------------------------------------------------------------------

bool DebugKey::isPressed(int)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ----------------------------------------------------------------------

bool DebugKey::isDown(int)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ======================================================================

Material::Material()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

Material::~Material()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

MemoryBlockManager::MemoryBlockManager(const char *, bool, int, int, int, int)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

MemoryBlockManager::~MemoryBlockManager()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

int MemoryBlockManager::getElementSize() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

void *MemoryBlockManager::allocate(bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

void MemoryBlockManager::free(void *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

bool Os::isMainThread(void)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

Os::ThreadId Os::getThreadId()
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

// ======================================================================

Transform const Transform::identity;

void Transform::multiply(const Transform &, const Transform &)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

const char *Shader::getName() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return NULL;
}

bool StaticShader::getMaterial(Tag, Material &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getTextureData(Tag, StaticShaderTemplate::TextureData &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getTexture(Tag, const Texture *&) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getTextureCoordinateSet(Tag, uint8 &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getTextureFactor(Tag, uint32 &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getTextureScroll(Tag, StaticShaderTemplate::TextureScroll &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getAlphaTestReferenceValue(Tag, uint8 &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::getStencilReferenceValue(Tag, uint32 &) const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

bool StaticShader::containsPrecalculatedVertexLighting() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ======================================================================

void Texture::fetch() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void Texture::release() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

DynamicIndexBufferGraphicsData::~DynamicIndexBufferGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

DynamicVertexBufferGraphicsData::~DynamicVertexBufferGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

HardwareIndexBuffer::~HardwareIndexBuffer()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

StaticIndexBuffer::StaticIndexBuffer(int)
: HardwareIndexBuffer(T_static)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

StaticIndexBuffer::~StaticIndexBuffer()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

StaticIndexBufferGraphicsData::~StaticIndexBufferGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

StaticShaderGraphicsData::~StaticShaderGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

ShaderImplementationGraphicsData::~ShaderImplementationGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

StaticVertexBufferGraphicsData::~StaticVertexBufferGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

TextureGraphicsData::~TextureGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

VertexBufferVectorGraphicsData::~VertexBufferVectorGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

ShaderImplementationPassVertexShaderGraphicsData::~ShaderImplementationPassVertexShaderGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

ShaderImplementationPassPixelShaderProgramGraphicsData::~ShaderImplementationPassPixelShaderProgramGraphicsData()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

char const * ShaderImplementationPassPixelShaderProgram::getFileName() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

int ShaderImplementationPassPixelShaderProgram::getVersionMajor() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

int ShaderImplementationPassPixelShaderProgram::getVersionMinor() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

// ======================================================================

int ConfigFile::getKeyInt(const char *, const char *, int, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0;
}

bool  ConfigFile::getKeyBool  (const char *, const char *, bool, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return false;
}

// ======================================================================

real Clock::frameTime()
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0.0f;
}

// ======================================================================

void Profiler::enter(char const *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void Profiler::leave(char const *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void Profiler::transfer(char const *, char const *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

AbstractFile *TreeFile::open(const char *, AbstractFile::PriorityType, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return NULL;
}

// ======================================================================

CrcString::CrcString()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

CrcString::~CrcString()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

bool CrcString::operator < (CrcString const &) const
{
	return false;
}

// ======================================================================

PersistentCrcString::PersistentCrcString(CrcString const &)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

PersistentCrcString::~PersistentCrcString()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}


char const * PersistentCrcString::getString() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return NULL;
}

void PersistentCrcString::clear()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void PersistentCrcString::set(char const *, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void PersistentCrcString::set(char const *, uint32)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

TemporaryCrcString::TemporaryCrcString(char const *, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

TemporaryCrcString::~TemporaryCrcString()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

char const * TemporaryCrcString::getString() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return NULL;
}

void TemporaryCrcString::clear()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void TemporaryCrcString::set(char const *, bool)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void TemporaryCrcString::set(char const *, uint32)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

void Graphics::setLastError(char const *, char const *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

// ======================================================================

bool Graphics::writeImage(char const *, int const, int const, int const, int const *, bool const, Gl_imageFormat const, Rectangle2d const *)
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return true;
}

// ======================================================================

void CrashReportInformation::addStaticText(char const *, ...)
{
	DLLEXPORT_UNREACHABLE_TRAP();
}


// ======================================================================

PerformanceTimer::PerformanceTimer()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

PerformanceTimer::~PerformanceTimer()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void PerformanceTimer::start()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void PerformanceTimer::resume()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

void PerformanceTimer::stop()
{
	DLLEXPORT_UNREACHABLE_TRAP();
}

float PerformanceTimer::getElapsedTime() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return 0.0f;
}

// ======================================================================

Transform const & Object::getTransform_o2w() const
{
	DLLEXPORT_UNREACHABLE_TRAP();
	return Transform::identity;
}

// ======================================================================

BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID)
{
	return TRUE;
}
