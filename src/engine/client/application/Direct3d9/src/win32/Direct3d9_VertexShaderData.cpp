// ======================================================================
//
// Direct3d9_VertexShaderData.cpp
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstDirect3d9.h"
#include "Direct3d9_VertexShaderData.h"

#ifdef VSPS

#include "ConfigDirect3d9.h"
#include "Direct3d9.h"
#include "Direct3d9_PixelShaderConstantRegisters.h"
#include "Direct3d9_VertexShaderConstantRegisters.h"
#include "Direct3d9_VertexShaderVertexRegisters.h"
#include "Direct3d9_HlslRewrite.h"   // Fix B (Phase 32): shared HLSL textual rewrite (Rules A/B/C) for the D3DCompile path
#include "clientGraphics/ShaderCapability.h"
#include "fileInterface/AbstractFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/LessPointerComparator.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedFoundation/PersistentCrcString.h"

#include <map>
#include <vector>
#include <float.h>   // Phase 19: _clearfp / _fpreset for the SEH-guarded compile (FP-fault fix)
#include <d3dx9.h>
#include <d3dx9shader.h>
#include <d3dcompiler.h>   // Fix B (Phase 32): D3DCompile replaces D3DXCompileShader on the HLSL compile path

// ======================================================================

namespace Direct3d9_VertexShaderDataNamespace
{
	struct Include
	{
	public:

		Include(CrcString const &fileName, AbstractFile *file);
		~Include();

	public:

		PersistentCrcString  m_fileName;
		int                  m_length;
		byte *               m_data;

	private:

		Include(Include const &);
		Include & operator =(Include const &);
	};

	class IncludeHandler : public ID3DXInclude
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
		virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData);
	};

	typedef std::vector<D3DXMACRO>  Defines;
	typedef std::map<CrcString const *, Include *, LessPointerComparator> IncludeCache;

	void getToken(char const *& s, char * d);
	void skipRestOfTheLine(char const *& s);

	const Tag TAG_DOT3 = TAG(D,O,T,3);

	// these exist to avoid disk access and memory allocations creating shaders
	Defines        ms_defines;
	char           ms_scratchBuffer[2 * 1024];
	IncludeCache * ms_includeCache;

	// ------------------------------------------------------------------
	// Phase 32 Fix B (2026-06-16): the HLSL `//hlsl` .vsh compile path now runs through
	// D3DCompile (d3dcompiler_47), NOT D3DXCompileShader. D3DX was statically compiled FROM
	// SOURCE into the D3D9 plugin (gl05/gl07); built with the modern MSVC toolchain its shader
	// compiler unmasked FP exceptions and faulted (0xC0000090) during its post-compile FP
	// cleanup on certain bump/dot3 vertex shaders -- the retail VS2003-built D3DX did not.
	// D3DCompile does not have that bug (the D3D11 path has used it since Phase 11). The
	// textual modernization needed by the strict modern compiler (SM4+ reserved keywords,
	// stacked struct-member register bindings) is handled by Direct3d9_HlslRewrite (Rules
	// A/B/C; Rule D disabled for D3D9; Rule E gated OFF) applied to the main source and every
	// #include before this call.
	//
	// ABI: D3DXMACRO / ID3DXInclude / ID3DXBuffer are binary-layout-identical to the
	// d3dcompiler types (D3D_SHADER_MACRO / ID3DInclude / ID3DBlob), so the surrounding
	// D3DX-typed code AND the separate D3DXAssembleShader asm path stay untouched; we
	// reinterpret-cast only at this single call boundary (RESEARCH Pattern 1).
	//
	// Fix-A SEH guard RETAINED (D-04 / review fix #E): this plan keeps __try/__except around
	// the compile until Wave 2 (32-05) confirms BOTH the HLSL and asm paths render clean off
	// D3DX. D3DCompile is immune to the D3DX FP fault, so the guard is now belt-and-suspenders
	// -- it stays so the Fix-A removal is a single isolated Wave-2 change, not bundled here.
	// __try/__except cannot live in a function needing C++ object unwinding (C2712), hence the
	// helper. One guard covers all VSPS plugins (gl05/07).
	inline int direct3d9CompileFpExceptionFilter(unsigned long code)
	{
		// STATUS_FLOAT_* SEH codes occupy 0xC000008D..0xC0000093 (denormal..underflow).
		return (code >= 0xC000008DUL && code <= 0xC0000093UL) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
	}

	HRESULT compileVertexShaderFpGuarded(
		LPCSTR src, UINT srcLen, D3DXMACRO const *defines, LPD3DXINCLUDE includeHandler,
		LPCSTR functionName, LPCSTR profile, DWORD flags,
		LPD3DXBUFFER *outShader, LPD3DXBUFFER *outErrors)
	{
		HRESULT hr = E_FAIL;
		__try
		{
			// Fix B: D3DCompile via the ABI-identical reinterpret-cast at the call boundary.
			// `flags` carries D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY ONLY -- explicitly NOT
			// D3DCOMPILE_PACK_MATRIX_ROW_MAJOR (review fix #4): the D3D9 path uses explicit
			// transposed constant uploads, not a row-major cbuffer contract; copying gl11's
			// row-major flag would silently change matrix packing -> wrong transforms.
			hr = D3DCompile(
				src, srcLen, NULL,
				reinterpret_cast<D3D_SHADER_MACRO const *>(defines),
				reinterpret_cast<ID3DInclude *>(includeHandler),
				functionName, profile, flags, 0,
				reinterpret_cast<ID3DBlob **>(outShader),
				reinterpret_cast<ID3DBlob **>(outErrors));
			_clearfp();
		}
		__except (direct3d9CompileFpExceptionFilter(GetExceptionCode()))
		{
			// Belt-and-suspenders: D3DCompile is immune to the D3DX FP fault, but keep the
			// recovery path until Fix-A is formally retired in Wave 2. If a fault somehow
			// occurs, the bytecode is fully produced before any post-compile FP cleanup;
			// reset the FPU and report success when we have bytecode.
			_fpreset();
			_clearfp();
			hr = (outShader && *outShader) ? S_OK : E_FAIL;
		}
		return hr;
	}
}
using namespace Direct3d9_VertexShaderDataNamespace;

// ======================================================================

#define SCRATCH_STRING(a, b)	strcpy(scratchBuffer, a); DEBUG_FATAL(b != strlen(a), ("wrong string length (was: %d should: %d", b, strlen(a))); scratchBuffer += b
#define SCRATCH_TAG(a)	      ConvertTagToString(a, scratchBuffer); scratchBuffer += 4
#define SCRATCH_INT(a)	      _itoa(a, scratchBuffer, 10); scratchBuffer += strlen(scratchBuffer)
#define SCRATCH_DONE()	      *scratchBuffer = '\0'; scratchBuffer += 1

// ======================================================================

Direct3d9_VertexShaderDataNamespace::Include::Include(CrcString const &fileName, AbstractFile * file)
:
	m_fileName(fileName),
	m_length(0),
	m_data(0)
{
	// Fix B (Phase 32) -- include-cache rewrite-BEFORE-insert (review fix Cursor C2):
	// Unlike gl11 (which rewrites on EVERY Open and never caches), the D3D9 IncludeHandler
	// caches the include bytes. Apply Direct3d9_HlslRewrite to the resolved .inc text HERE,
	// in the Include ctor, so the REWRITTEN bytes are what get stored in ms_includeCache
	// (the ctor runs before the cache insert in IncludeHandler::Open). This guarantees the
	// 2nd #include of the same module (e.g. vertex_shader_constants.inc) returns rewritten
	// text on the cache-hit path too -- never the raw stacked `: register()` D3DCompile rejects.
	// applyToIncludeBuffer takes ownership of the readEntireFileAndClose() buffer (delete[]s it
	// on rewrite) and returns a buffer we own + free in ~Include via delete[]. Capture the raw
	// length BEFORE the read closes the file.
	int const rawLength = file->length();
	std::size_t outLength = 0;
	m_data = reinterpret_cast<byte *>(Direct3d9_HlslRewrite::applyToIncludeBuffer(
		reinterpret_cast<unsigned char *>(file->readEntireFileAndClose()),
		static_cast<std::size_t>(rawLength), outLength, m_fileName.getString()));
	m_length = static_cast<int>(outLength);
}

// ----------------------------------------------------------------------

Direct3d9_VertexShaderDataNamespace::Include::~Include()
{
	delete [] m_data;
}

// ======================================================================

HRESULT Direct3d9_VertexShaderDataNamespace::IncludeHandler::Open(D3DXINCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes)
{
	// hack to support relative path includes when using the command line compiler
	if (strncmp(pFileName, "../../", 6) == 0)
		pFileName += 6;

	if (Direct3d9::engineOwnsWindow())
	{
		TemporaryCrcString findFileName(pFileName, true);
		IncludeCache::iterator i = ms_includeCache->find(&findFileName);
		if (i == ms_includeCache->end())
		{
			AbstractFile *file = TreeFile::open(pFileName, AbstractFile::PriorityData, true);
			if (!file)
			{
				DEBUG_FATAL(true, ("could not open include %s", pFileName));
				return STG_E_FILENOTFOUND;
			}

			Include *include = new Include(findFileName, file);
			delete file;

			i = ms_includeCache->insert(IncludeCache::value_type(&include->m_fileName, include)).first;
		}

		*ppData = reinterpret_cast<void*>(i->second->m_data);
		*pBytes = i->second->m_length;
	}
	else
	{
		AbstractFile *file = TreeFile::open(pFileName, AbstractFile::PriorityData, true);
		if (!file)
		{
			DEBUG_FATAL(true, ("could not open include %s", pFileName));
			return STG_E_FILENOTFOUND;
		}

		// Fix B (Phase 32): non-engine-owns (viewer/tool) path -- rewrite the resolved include
		// too, so D3DCompile sees the same modernized text as the cached engine path. The matching
		// Close() delete[]s whatever pointer we return; applyToIncludeBuffer owns the read buffer.
		int const rawLength = file->length();
		unsigned char *raw = reinterpret_cast<unsigned char *>(file->readEntireFileAndClose());
		delete file;
		std::size_t outLength = 0;
		*ppData = Direct3d9_HlslRewrite::applyToIncludeBuffer(raw, static_cast<std::size_t>(rawLength), outLength, pFileName);
		*pBytes = static_cast<UINT>(outLength);
	}

	return S_OK;
}

// ----------------------------------------------------------------------

HRESULT Direct3d9_VertexShaderDataNamespace::IncludeHandler::Close(LPCVOID data)
{
	// this is a hack to unload include files for the viewer so that it can be used for interactive shader editing
	if (!Direct3d9::engineOwnsWindow())
		delete [] (const_cast<byte *>(reinterpret_cast<byte const *>(data)));

	return 0;
}

// ----------------------------------------------------------------------

void Direct3d9_VertexShaderDataNamespace::getToken(const char *& s, char *d)
{
	// handle end of the buffer
	if (s == 0 || *s == '\0')
	{
		*d = '\0';
		return;
	}

	// skip leading whitespace
	while (*s && (*s == ' ' || *s == '\r' || *s == '\n' || *s == '\t'))
		++s;

	// copy characters until a NUL or whitespace
	for ( ; *s && *s != ' ' && *s != '\n' && *s != '\r' && *s != '\t'; ++s, ++d)
		*d = *s;

	// terminate the destination buffer
	*d = '\0';
}

// ----------------------------------------------------------------------

void Direct3d9_VertexShaderDataNamespace::skipRestOfTheLine(const char *& s)
{
	// handle end of the buffer
	if (s == 0 || *s == '\0')
		return;

	// skip characters until a NUL or whitespace
	for ( ; *s && *s != '\n' && *s != '\r' ; )
		if (*s == '\\')
		{
			// handle a quoted newline
			++s;
			if (*s == '\r')
				++s;
			if (*s == '\n')
				++s;
		}
		else
		{
			++s;
		}
}

// ======================================================================

void Direct3d9_VertexShaderData::install()
{
	ms_defines.reserve(32);
	ms_includeCache = new IncludeCache;
}

// ----------------------------------------------------------------------

void Direct3d9_VertexShaderData::remove()
{
	if (ms_includeCache)
	{
		while (!ms_includeCache->empty())
		{
			IncludeCache::iterator i = ms_includeCache->begin();
			Include *include = i->second;
			ms_includeCache->erase(i);
			delete include;
		}
		delete ms_includeCache;
	}
}

// ----------------------------------------------------------------------

Direct3d9_VertexShaderData::Direct3d9_VertexShaderData(ShaderImplementation::Pass::VertexShader const & vertexShader)
: ShaderImplementationPassVertexShaderGraphicsData(),
	m_vertexShader(&vertexShader),
	m_hlsl(false),
	m_compileText(NULL),
	m_compileTextLength(0),
	m_textureCoordinateSetTags(NULL),
	m_container(NULL),
	m_nonPatchedVertexShader(NULL),
	m_lastRequestedKey(0xFFFFFFFF),
	m_lastReturnedValue(NULL)
{
	char const * text = m_vertexShader->m_text;

	// here's two samples of the data we're parsing
	/*

	//hlsl vs_1_1
	#define textureCoordinateSetMAIN        textureCoordinateSet0
	#define DECLARE_textureCoordinateSet0   float2 textureCoordinateSet0 : TEXCOORD0 : register(v7);

	//asm vs_1_1
	#define maxTextureCoordinate      2
	#define vTextureCoordinateSetDTLA vTextureCoordinateSet0
	#define vTextureCoordinateSetDTLB vTextureCoordinateSet1
	#define vTextureCoordinateSetMASK vTextureCoordinateSet2

	*/

	bool assembly = false;
	for (bool done = false; !done; )
	{
		// remember where we were before the first bad token
		m_compileText = text;

		char token[128];
		getToken(text, token);

		if (strcmp(token, "//hlsl") == 0)
		{
			m_hlsl = true;
			skipRestOfTheLine(text);
		}
		else if (strcmp(token, "//asm") == 0)
		{
			assembly = true;
			skipRestOfTheLine(text);
		}
		else if (strcmp(token, "#define") == 0)
		{
			getToken(text, token);

			char * tag = 0;
			if (m_hlsl)
			{
				if (strncmp(token, "textureCoordinateSet", 20) == 0)
					tag = token + 20;
			}
			if (assembly)
			{
				if (strncmp(token, "vTextureCoordinateSet", 21) == 0)
					tag = token + 21;
			}

			if (tag)
			{
				if (!m_textureCoordinateSetTags)
					m_textureCoordinateSetTags = new TextureCoordinateSetTags();
				m_textureCoordinateSetTags->push_back(ConvertStringToTag(tag));
			}

			skipRestOfTheLine(text);
		}
		else
		{
			done = true;
		}
	}

	DEBUG_FATAL(!assembly && !m_hlsl, ("Could not determine shader language for %s",  m_vertexShader->getFilename()));
	m_compileTextLength = strlen(m_compileText);
}

// ----------------------------------------------------------------------

Direct3d9_VertexShaderData::~Direct3d9_VertexShaderData()
{
	m_vertexShader = NULL;

	if (m_nonPatchedVertexShader)
	{
		m_nonPatchedVertexShader->Release();
		m_nonPatchedVertexShader = NULL;
	}

	if (m_container)
	{
		while (!m_container->empty())
		{
			m_container->begin()->second->Release();
			m_container->erase(m_container->begin());
		}
		delete m_container;
	}

	delete m_textureCoordinateSetTags;
	m_compileText = 0;
}

// ----------------------------------------------------------------------

IDirect3DVertexShader9 * Direct3d9_VertexShaderData::createVertexShader(uint32 textureCoordinateSetKey) const
{
	ID3DXBuffer *compiledShader = NULL;

	ms_defines.clear();
	char const * target = NULL;
	if (Direct3d9::getShaderCapability() >= ShaderCapability(2,0))
	{
		target = "vs_2_0";
	}
	else
	{
		// Fix B (Phase 32 / review fix #5 / D-06): D3DCompile (d3dcompiler_47) DROPPED vs_1_1
		// support. The legacy `target = "vs_1_1"` here would hand D3DCompile a profile it rejects
		// -> a compile FATAL or (worse) a silently-nulled VS / skipped draw on a <SM2.0 machine.
		// Promote unconditionally to vs_2_0 so the path can never compile-FATAL on the profile and
		// never silently null a VS. A genuine <2.0 GPU is unsupported on this modern toolchain; the
		// DEBUG_FATAL makes that assumption loud in the Debug build (the HLSL path only ever runs
		// on shader-capable hardware; getShaderCapability() < 2.0 is not a configuration we ship).
		DEBUG_FATAL(true, ("Direct3d9_VertexShaderData: getShaderCapability() < 2.0 -- D3DCompile dropped vs_1_1; promoting to vs_2_0 (unsupported config)"));
		target = "vs_2_0";
	}

	const int numberOfTextureCoordinateSets = 8;
	if (m_hlsl)
	{
		if (m_textureCoordinateSetTags)
		{
			int textureCoordinateSetDimension[numberOfTextureCoordinateSets] = { 0, 0, 0, 0, 0, 0, 0, 0 };

			char *scratchBuffer = ms_scratchBuffer;
			{
				int const size = m_textureCoordinateSetTags->size();
				for (int i = 0; i < size; ++i)
				{
					int const textureCoordinateSet = (textureCoordinateSetKey >> (i * 3)) & 7;
					int const dimension = ((*m_textureCoordinateSetTags)[i] == TAG_DOT3) ? 4 : 2;
					DEBUG_FATAL(textureCoordinateSetDimension[textureCoordinateSet] != 0 && textureCoordinateSetDimension[textureCoordinateSet] != dimension, ("Competing dimensions (existing %d, new %d) for texture coordinate %d", textureCoordinateSetDimension[textureCoordinateSet], dimension, textureCoordinateSet));
					textureCoordinateSetDimension[textureCoordinateSet] = dimension;

					D3DXMACRO macro;

					// here's an example of what we are defining:
					// #define textureCoordinateSetMAIN textureCoordinateSet0

					macro.Name = scratchBuffer;
					SCRATCH_STRING("textureCoordinateSet", 20);
					SCRATCH_TAG((*m_textureCoordinateSetTags)[i]);
					SCRATCH_DONE();

					macro.Definition = scratchBuffer;
					SCRATCH_STRING("textureCoordinateSet", 20);
					SCRATCH_INT(textureCoordinateSet);
					SCRATCH_DONE();

					ms_defines.push_back(macro);
				}
			}

			{
				// here's an example of what we are defining:
				// #define DECLARE_textureCoordinateSets  float2 textureCoordinateSet0 : TEXCOORD0 : register(v7);

				D3DXMACRO macro;

				macro.Name = scratchBuffer;
				SCRATCH_STRING("DECLARE_textureCoordinateSets", 29);
				SCRATCH_DONE();

				macro.Definition = scratchBuffer;

				for (int i = 0; i < numberOfTextureCoordinateSets; ++i)
					if (textureCoordinateSetDimension[i])
					{

						SCRATCH_STRING("float", 5);
						SCRATCH_INT(textureCoordinateSetDimension[i]);
						SCRATCH_STRING(" textureCoordinateSet", 21);
						SCRATCH_INT(i);
						SCRATCH_STRING(" : TEXCOORD", 11);
						SCRATCH_INT(i);
						// Fix B (Phase 32 / review fix #7 / Pitfall 3): OMIT the D3D9-era
						// ": register(vN)" on this struct member. Modern fxc (d3dcompiler_47)
						// rejects a register binding on a struct member (X3202) even at vs_2_0.
						// This macro expands INSIDE D3DCompile AFTER applyToMainSource ran on the
						// raw source -- the pre-preprocessor Rules B/C never see this post-expansion
						// text, so the omission must be made HERE. The TEXCOORD<i> semantic alone
						// drives input binding; the gapped VSVR v# slot (v7..v14) is honored by the
						// D3DVERTEXELEMENT9 stream decl, NOT by this inline register hint (Task-2(G)
						// input-signature audit proves the dcl signature still matches VSVR).
						SCRATCH_STRING(";", 1);
					}

				SCRATCH_DONE();

				ms_defines.push_back(macro);
			}

			DEBUG_FATAL(ms_scratchBuffer + sizeof(ms_scratchBuffer) < scratchBuffer, ("Scratch buffer overflow"));
		}

		{
			D3DXMACRO empty = { NULL, NULL };
			ms_defines.push_back(empty);
		}

		IncludeHandler includeHandler;
		ID3DXBuffer *error = NULL;
		// Fix B (Phase 32): apply Rule A (and the conditional rules) to the MAIN shader source
		// before compiling -- the #includes are rewritten in IncludeHandler::Open / the Include
		// ctor. Then compile via D3DCompile (inside the retained Fix-A SEH guard --
		// compileVertexShaderFpGuarded now wraps D3DCompile). Flags =
		// D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY ONLY (NO PACK_MATRIX_ROW_MAJOR -- review fix #4).
		std::vector<char> rewrittenSource;
		Direct3d9_HlslRewrite::applyToMainSource(m_compileText, static_cast<std::size_t>(m_compileTextLength), rewrittenSource, m_vertexShader->m_fileName.getString());
		HRESULT result = compileVertexShaderFpGuarded(rewrittenSource.data(), static_cast<UINT>(rewrittenSource.size()), &(ms_defines.front()), &includeHandler, "main", target, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, &compiledShader, &error);

		//-----------------------------------------------------------------------------------
		// DBE - I was getting strange Float Invalid Operation Exceptions (0xC0000090) in the
		// Debug Build on floating point instructions which I eventually traced back to the FPU
		// being left in some bad state after a call to D3DXCompileShader (on a certain .vsh).
		// I also found that calling '_clearfp()' cleared up the problem.
		// (Retained belt-and-suspenders; the SEH guard above is the actual fix for the fault
		//  that occurs INSIDE the call, which this post-call _clearfp cannot reach.)
		_clearfp();
		//-----------------------------------------------------------------------------------

		FATAL(FAILED(result), ("Could not compile shader %s %d (%s)", m_vertexShader->m_fileName.getString(), HRESULT_CODE(result), error ? error->GetBufferPointer() : "none"));
		if (error)
		{
			DEBUG_REPORT_LOG_PRINT(true, ("%s", error->GetBufferPointer()));
			error->Release();
		}
	}
	else
	{
		if (m_textureCoordinateSetTags)
		{
#ifdef _DEBUG
			int textureCoordinateSetDimension[numberOfTextureCoordinateSets] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#endif

			int maxTextureCoordinateSet = -1;

			char *scratchBuffer = ms_scratchBuffer;
			int const size = m_textureCoordinateSetTags->size();
			for (int i = 0; i < size; ++i)
			{
				int const textureCoordinateSet = (textureCoordinateSetKey >> (i * 3)) & 7;

#ifdef _DEBUG
				int const dimension = ((*m_textureCoordinateSetTags)[i] == TAG_DOT3) ? 4 : 2;
				DEBUG_FATAL(textureCoordinateSetDimension[textureCoordinateSet] != 0 && textureCoordinateSetDimension[textureCoordinateSet] != dimension, ("Competing dimensions (existing %d, new %d) for texture coordinate %d", textureCoordinateSetDimension[textureCoordinateSet], dimension, textureCoordinateSet));
				textureCoordinateSetDimension[textureCoordinateSet] = dimension;
#endif

				if (textureCoordinateSet > maxTextureCoordinateSet)
					maxTextureCoordinateSet = textureCoordinateSet;

				D3DXMACRO macro;

				// here's an example of what we are defining:
				// #define vTextureCoordinateSetDTLA vTextureCoordinateSet0

				macro.Name = scratchBuffer;
				SCRATCH_STRING("vTextureCoordinateSet", 21);
				SCRATCH_TAG((*m_textureCoordinateSetTags)[i]);
				SCRATCH_DONE();

				macro.Definition = scratchBuffer;
				SCRATCH_STRING("vTextureCoordinateSet", 21);
				SCRATCH_INT(textureCoordinateSet);
				SCRATCH_DONE();

				ms_defines.push_back(macro);
			}

			{
				// here's an example of what we are defining:
				// #define maxTextureCoordinate 2
				D3DXMACRO macro;

				macro.Name = "maxTextureCoordinate";

				macro.Definition = scratchBuffer;
				SCRATCH_INT(maxTextureCoordinateSet);
				SCRATCH_DONE();

				ms_defines.push_back(macro);
			}

			DEBUG_FATAL(ms_scratchBuffer + sizeof(ms_scratchBuffer) < scratchBuffer, ("Scratch buffer overflow"));
		}

		{
			D3DXMACRO d = { "TARGET", target };
			ms_defines.push_back(d);
		}

		{
			D3DXMACRO empty = { NULL, NULL };
			ms_defines.push_back(empty);
		}

		IncludeHandler includeHandler;
		HRESULT result = D3DXAssembleShader(m_compileText, m_compileTextLength, &(ms_defines.front()), &includeHandler, 0, &compiledShader, NULL);
		FATAL(FAILED(result), ("Could not compile shader %s %d", m_vertexShader->m_fileName.getString(), HRESULT_CODE(result)));
	}

	// create the vertex shader	from the binary token stream
	IDirect3DVertexShader9 *vertexShader = NULL;
	HRESULT const hresult = Direct3d9::getDevice()->CreateVertexShader(reinterpret_cast<DWORD const *>(compiledShader->GetBufferPointer()), &vertexShader);
	FATAL_DX_HR("CreateVertexShader failed %s", hresult);
	NOT_NULL(vertexShader);
	compiledShader->Release();

	return vertexShader;
}

// ----------------------------------------------------------------------

IDirect3DVertexShader9 * Direct3d9_VertexShaderData::getVertexShader(uint32 textureCoordinateSetKey) const
{
	if(m_lastRequestedKey == textureCoordinateSetKey)
		return m_lastReturnedValue;
#ifdef _DEBUG
	if (!ConfigDirect3d9::getCreateShaders())
		return 0;
#endif

	// non-patched vertex shaders can always return the same value
	if (!m_textureCoordinateSetTags)
	{
		if (!m_nonPatchedVertexShader)
			m_nonPatchedVertexShader = createVertexShader(0);

		return m_nonPatchedVertexShader;
	}

	if (!m_container)
		m_container = new Container();

	// see if we have a vertex shader already created for this set of texture coordinate sets
	Container::const_iterator i = m_container->find(textureCoordinateSetKey);
	if (i == m_container->end())
	{
		// create it and put it into the map
		i = m_container->insert(Container::value_type(textureCoordinateSetKey, createVertexShader(textureCoordinateSetKey))).first;
	}

	m_lastRequestedKey = textureCoordinateSetKey;
	m_lastReturnedValue = i->second;
	return i->second;
}

// ======================================================================

#endif
