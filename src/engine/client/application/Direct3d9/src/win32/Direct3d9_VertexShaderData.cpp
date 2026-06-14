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
#include "clientGraphics/ShaderCapability.h"
#include "fileInterface/AbstractFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/LessPointerComparator.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedFoundation/PersistentCrcString.h"

#include <map>
#include <vector>
#include <float.h>   // Phase 19: _clearfp / _fpreset for the SEH-guarded D3DX compile (FP-fault fix)
#include <d3dx9.h>        // Phase 27: retained -- asm path (D3DXAssembleShader) + D3DX matrix/surface APIs in sibling files
#include <d3dx9shader.h>  // Phase 27: retained -- asm path still uses D3DX
#include <d3dcompiler.h>  // Phase 27 (HARD-05 Fix B): D3DCompile / ID3DInclude / D3D_SHADER_MACRO for the HLSL VS compile

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

	class IncludeHandler : public ID3DInclude
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
		virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData);
	};

	typedef std::vector<D3D_SHADER_MACRO>  Defines;
	typedef std::map<CrcString const *, Include *, LessPointerComparator> IncludeCache;

	void getToken(char const *& s, char * d);
	void skipRestOfTheLine(char const *& s);

	const Tag TAG_DOT3 = TAG(D,O,T,3);

	// these exist to avoid disk access and memory allocations creating shaders
	Defines        ms_defines;
	char           ms_scratchBuffer[2 * 1024];
	IncludeCache * ms_includeCache;

	// ------------------------------------------------------------------
	// Phase 19 fix (2026-05-31): the D3DX library is statically compiled FROM SOURCE into the
	// D3D9 plugin (gl05/gl07). Built with the modern MSVC toolchain its shader compiler unmasks
	// FP exceptions and trips an invalid-op / divide-by-zero during its post-compile FP cleanup
	// (the FWAIT inside _control87, observed as 0xC0000090) on certain vertex shaders -- the
	// retail VS2003-built D3DX did not. Confirmed by control test: the pristine SWG Source
	// VS2003 client renders the same shader/asset/spot without faulting; ours crashes. The shader
	// bytecode is fully produced BEFORE the cleanup fault, so wrap the call in SEH, reset the FPU,
	// and keep the compiled result. Extracted to a helper because __try/__except cannot live in a
	// function that needs C++ object unwinding (C2712). One fix covers all VSPS plugins (gl05/07).
	// TODO (Fix B, deferred): port D3D9 shader compile to D3DCompile, which is immune to this
	// D3DX-era FPU bug (the D3D11 path already uses it). See .planning todo.
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
			hr = D3DXCompileShader(src, srcLen, defines, includeHandler, functionName, profile, flags, outShader, outErrors, NULL);
			_clearfp();
		}
		__except (direct3d9CompileFpExceptionFilter(GetExceptionCode()))
		{
			// D3DX faulted during its post-compile FP cleanup; *outShader already holds the
			// compiled bytecode. Reset the FPU to a sane masked state and report success when
			// we have bytecode (mirrors the no-fault path's result on the old toolchain).
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
	m_length(file->length()),
	m_data(file->readEntireFileAndClose())
{
}

// ----------------------------------------------------------------------

Direct3d9_VertexShaderDataNamespace::Include::~Include()
{
	delete [] m_data;
}

// ======================================================================

HRESULT Direct3d9_VertexShaderDataNamespace::IncludeHandler::Open(D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes)
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

		*pBytes = file->length();
		*ppData = file->readEntireFileAndClose();
		delete file;
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
		target = "vs_2_0";
	else
		target = "vs_1_1";

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

					D3D_SHADER_MACRO macro;

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

				D3D_SHADER_MACRO macro;

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
						SCRATCH_STRING(" : register(v", 13);
 						SCRATCH_INT(VSVR_textureCoordinateSet0 + i);
						SCRATCH_STRING(");", 2);
					}

				SCRATCH_DONE();

				ms_defines.push_back(macro);
			}

			DEBUG_FATAL(ms_scratchBuffer + sizeof(ms_scratchBuffer) < scratchBuffer, ("Scratch buffer overflow"));
		}

		{
			D3D_SHADER_MACRO empty = { NULL, NULL };
			ms_defines.push_back(empty);
		}

		IncludeHandler includeHandler;
		ID3DXBuffer *error = NULL;
		// Phase 19: SEH-guarded to survive the modern-toolchain D3DX post-compile FP fault
		// (0xC0000090) -- see compileVertexShaderFpGuarded above.
		HRESULT result = compileVertexShaderFpGuarded(m_compileText, m_compileTextLength, &(ms_defines.front()), &includeHandler, "main", target, 0, &compiledShader, &error);

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

				D3D_SHADER_MACRO macro;

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
				D3D_SHADER_MACRO macro;

				macro.Name = "maxTextureCoordinate";

				macro.Definition = scratchBuffer;
				SCRATCH_INT(maxTextureCoordinateSet);
				SCRATCH_DONE();

				ms_defines.push_back(macro);
			}

			DEBUG_FATAL(ms_scratchBuffer + sizeof(ms_scratchBuffer) < scratchBuffer, ("Scratch buffer overflow"));
		}

		{
			D3D_SHADER_MACRO d = { "TARGET", target };
			ms_defines.push_back(d);
		}

		{
			D3D_SHADER_MACRO empty = { NULL, NULL };
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
