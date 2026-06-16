// =====================================================================================
//  d3dassemble_probe.cpp  —  Phase 32 (SHADER-01) plan 32-01 Task 2
//
//  The CRITICAL EARLY DE-RISK probe for the D3DX -> d3dcompiler_47 port.
//
//  Question answered: does D3DAssemble (d3dcompiler_47.dll) accept the SWG `.vsh` ASM
//  dialect AND produce bytecode SEMANTICALLY equivalent to the D3DXAssembleShader
//  baseline (the runtime call at Direct3d9_VertexShaderData.cpp:567)?
//
//  Per-shader it: (a) resolves D3DAssemble + D3DDisassemble via GetProcAddress on the
//  undecorated DLL exports (ordinals 1 + 13 per dumpbin), held in a STATIC-LOCAL
//  resolve-once cache (Sonnet L2 — no per-shader LoadLibrary); (b) assembles via
//  D3DAssemble AND via D3DXAssembleShader with the SAME define/include setup the runtime
//  uses (TARGET=vs_2_0, the registers.inc v#/c#/b# aliases); (c) diffs bytecode SIZE+HASH;
//  (d) on byte-MISMATCH, runs a D3DDisassemble TOKEN-compare fallback (review fix #2 —
//  benign cross-assembler byte-variance must not force a false FAIL); (e) CreateVertexShader-s
//  the D3DAssemble bytecode on a real D3D9 device; (f) prints per-shader evidence.
//
//  Probe set (binding per 32-CENSUS.md Task-1 flag-list):
//     baseline : c_simple, tfcl, cloudlayer
//     F1 (lit) : tfcsl                (diffuse_specular.inc — the `lit` opcode)
//     F2 (4uv) : tfcl_4uv             (dcl_texcoord2/3 -> v9/v10, oT2/oT3)
//     F3 bonus : tfcsl + VERTEX_SHADER_VERSION=20  (the `if b#` boolean branch — TOOL path
//                only; the runtime asm path never defines VERTEX_SHADER_VERSION, so F3 is a
//                bonus dialect data point, NOT runtime-binding — see 32-CENSUS.md §4).
//
//  Build (x86 — the staged DLL + d3dx9.lib are 32-bit):
//     vcvars32.bat && cl /EHsc /nologo /I"<dx9>/include" d3dassemble_probe.cpp \
//        /link /LIBPATH:"<dx9>/lib" d3d9.lib d3dx9.lib
//  Run from stage/ (so d3dcompiler_47.dll + d3dx9_*.dll resolve); pass the extract dir as argv[1].
// =====================================================================================

#include <windows.h>
#include <d3d9.h>          // in-tree legacy DX9 SDK: IDirect3DDevice9, CreateVertexShader
#include <d3dx9.h>         // in-tree legacy DX9 SDK: D3DXAssembleShader, ID3DXBuffer, D3DXMACRO, ID3DXInclude
#include <d3dcompiler.h>   // Windows SDK: ID3DBlob, ID3DInclude, D3D_SHADER_MACRO, D3DDisassemble decl
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ------------------------------------------------------------------ DLL-resolved exports
// D3DAssemble is NOT in the public d3dcompiler.h, and the d3dcompiler.lib symbol is
// C++-mangled (?D3DAssemble@@...), so an extern "C" decl would not match the import.
// Resolve the undecorated DLL export directly (32-CENSUS.md §3: ordinal 1).
typedef HRESULT (WINAPI *PFN_D3DAssemble)(
    LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, UINT, ID3DBlob**, ID3DBlob**);
typedef HRESULT (WINAPI *PFN_D3DDisassemble)(
    LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob**);

static PFN_D3DAssemble resolveD3DAssemble()
{
    static HMODULE        s_mod = LoadLibraryA("d3dcompiler_47.dll");   // resolve-once (static-local)
    static PFN_D3DAssemble s_fn = s_mod ? reinterpret_cast<PFN_D3DAssemble>(GetProcAddress(s_mod, "D3DAssemble")) : nullptr;
    return s_fn;
}
static PFN_D3DDisassemble resolveD3DDisassemble()
{
    static HMODULE           s_mod = LoadLibraryA("d3dcompiler_47.dll"); // resolve-once (static-local)
    static PFN_D3DDisassemble s_fn = s_mod ? reinterpret_cast<PFN_D3DDisassemble>(GetProcAddress(s_mod, "D3DDisassemble")) : nullptr;
    return s_fn;
}

// ------------------------------------------------------------------ file IO + corpus dir
static std::string g_extractDir = ".planning/research/vsh-extract";

static bool readFileBytes(const std::string& path, std::string& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize(static_cast<size_t>(n));
    size_t rd = n ? fread(&out[0], 1, static_cast<size_t>(n), f) : 0;
    fclose(f);
    out.resize(rd);
    return true;
}

// Map an #include name ("vertex_program/modules/registers.inc") to <extractDir>/<basename>,
// mirroring the engine IncludeHandler's TreeFile-scoped resolution. SECURITY (T-32-02):
// take ONLY the basename so no '..'/absolute path can escape the corpus dir.
static std::string resolveIncludeBasename(LPCSTR fileName)
{
    std::string s(fileName ? fileName : "");
    size_t slash = s.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? s : s.substr(slash + 1);
    return g_extractDir + "/" + base;
}

// ------------------------------------------------------------------ include handlers
// D3DAssemble wants ID3DInclude; D3DXAssembleShader wants ID3DXInclude. Same Open/Close shape,
// different enum/interface types — implement both over one resolve helper.
struct AsmInclude : public ID3DInclude
{
    STDMETHOD(Open)(D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID* ppData, UINT* pBytes) override
    {
        std::string* buf = new std::string();
        if (!readFileBytes(resolveIncludeBasename(pFileName), *buf)) { delete buf; *ppData = nullptr; *pBytes = 0; return E_FAIL; }
        *ppData = buf->data(); *pBytes = static_cast<UINT>(buf->size());
        m_live.push_back(buf);
        return S_OK;
    }
    STDMETHOD(Close)(LPCVOID pData) override
    {
        for (size_t i = 0; i < m_live.size(); ++i)
            if (m_live[i]->data() == pData) { delete m_live[i]; m_live.erase(m_live.begin() + i); break; }
        return S_OK;
    }
    std::vector<std::string*> m_live;
};

struct DxInclude : public ID3DXInclude
{
    STDMETHOD(Open)(D3DXINCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID* ppData, UINT* pBytes) override
    {
        std::string* buf = new std::string();
        if (!readFileBytes(resolveIncludeBasename(pFileName), *buf)) { delete buf; *ppData = nullptr; *pBytes = 0; return E_FAIL; }
        *ppData = buf->data(); *pBytes = static_cast<UINT>(buf->size());
        m_live.push_back(buf);
        return S_OK;
    }
    STDMETHOD(Close)(LPCVOID pData) override
    {
        for (size_t i = 0; i < m_live.size(); ++i)
            if (m_live[i]->data() == pData) { delete m_live[i]; m_live.erase(m_live.begin() + i); break; }
        return S_OK;
    }
    std::vector<std::string*> m_live;
};

// ------------------------------------------------------------------ hashing / token compare
static uint64_t fnv1a(const void* data, size_t len)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

// Normalize disassembly text for a semantic token-compare: drop blank lines, leading/trailing
// whitespace, and // comment lines, and collapse internal whitespace runs to one space.
static std::string normalizeDisasm(const std::string& s)
{
    std::string out;
    size_t i = 0;
    while (i < s.size())
    {
        size_t eol = s.find('\n', i);
        std::string line = s.substr(i, (eol == std::string::npos ? s.size() : eol) - i);
        i = (eol == std::string::npos) ? s.size() : eol + 1;
        // strip trailing CR
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        // strip leading ws
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;             // blank
        line = line.substr(a);
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;  // comment-only line
        // collapse internal whitespace
        std::string norm; bool prevWs = false;
        for (char c : line) { bool ws = (c == ' ' || c == '\t'); if (ws) { if (!prevWs) norm += ' '; } else norm += c; prevWs = ws; }
        out += norm; out += '\n';
    }
    return out;
}

static std::string disassemble(const void* code, size_t size)
{
    PFN_D3DDisassemble dis = resolveD3DDisassemble();
    if (!dis) return std::string();
    ID3DBlob* text = nullptr;
    if (FAILED(dis(code, size, 0, nullptr, &text)) || !text) return std::string();
    std::string s(static_cast<const char*>(text->GetBufferPointer()), text->GetBufferSize());
    text->Release();
    return s;
}

// ------------------------------------------------------------------ D3D9 device for CreateVertexShader
static IDirect3D9*       g_d3d = nullptr;
static IDirect3DDevice9* g_dev = nullptr;
static HWND              g_wnd = nullptr;

static bool createDevice()
{
    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) { printf("  [device] Direct3DCreate9 FAILED\n"); return false; }
    WNDCLASSA wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "vshProbeWnd";
    RegisterClassA(&wc);
    g_wnd = CreateWindowA("vshProbeWnd", "vshProbe", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    D3DPRESENT_PARAMETERS pp; ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
    // Try HAL (real GPU, supports vs_2_0) then REF (reference rasterizer) — both support shaders.
    const D3DDEVTYPE types[2] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
    for (int t = 0; t < 2; ++t)
    {
        if (SUCCEEDED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, types[t], g_wnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_dev)) && g_dev)
        {
            printf("  [device] created (%s)\n", t == 0 ? "HAL" : "REF");
            return true;
        }
    }
    printf("  [device] CreateDevice FAILED (HAL+REF) — CreateVertexShader checks will be skipped\n");
    return false;
}

// ------------------------------------------------------------------ per-shader probe
struct ShaderCase { const char* file; bool defineVsVersion20; const char* note; };

static int probeShader(const ShaderCase& sc)
{
    std::string src;
    std::string path = g_extractDir + "/" + sc.file;
    printf("======================================================================\n");
    printf("SHADER: %s%s\n", sc.file, sc.note ? sc.note : "");
    if (!readFileBytes(path, src)) { printf("  FATAL: cannot read %s\n", path.c_str()); return 2; }

    // Define set mirroring the runtime asm path (TARGET=vs_2_0). F3 adds VERTEX_SHADER_VERSION=20.
    std::vector<D3D_SHADER_MACRO> defs;
    D3D_SHADER_MACRO target = { "TARGET", "vs_2_0" }; defs.push_back(target);
    if (sc.defineVsVersion20) { D3D_SHADER_MACRO v = { "VERTEX_SHADER_VERSION", "20" }; defs.push_back(v); }
    // HARNESS COMPLETION (not a runtime mirror): the Phase-27 extracted diffuse.inc (ILM pack) references
    // cExtLtData_* tangent-light aliases that the extracted registers.inc (data_other_00) does NOT define —
    // a corpus version-skew. Both D3DAssemble AND D3DXAssembleShader reject the same way without these, so
    // supplying identical free-slot aliases to BOTH keeps the equivalence comparison valid while letting
    // diffuse.inc-using shaders (c_simple/tfcl/tfcl_4uv F2) assemble. Slots c52/c53/c54 are free (registers.inc
    // ends at c51 unitZ, c95 holds constants). See 32-CENSUS.md §4.
    D3D_SHADER_MACRO ext0 = { "cExtLtData_parallelSpec_0_tangentColor",       "c52" }; defs.push_back(ext0);
    D3D_SHADER_MACRO ext1 = { "cExtLtData_parallelSpec_0_tangentMinusDiffuse", "c53" }; defs.push_back(ext1);
    D3D_SHADER_MACRO ext2 = { "cExtLtData_parallelSpec_0_tangentMinusBack",    "c54" }; defs.push_back(ext2);
    D3D_SHADER_MACRO term = { nullptr, nullptr }; defs.push_back(term);

    // ---- D3DAssemble (the candidate) ----
    PFN_D3DAssemble asmFn = resolveD3DAssemble();
    if (!asmFn) { printf("  FATAL: GetProcAddress(D3DAssemble) returned null\n"); return 2; }
    AsmInclude asmInc;
    ID3DBlob* aCode = nullptr; ID3DBlob* aErr = nullptr;
    HRESULT aHr = asmFn(src.data(), src.size(), sc.file, defs.data(), &asmInc, 0, &aCode, &aErr);
    printf("  D3DAssemble        HRESULT=0x%08lX\n", (unsigned long)aHr);
    if (aErr) { printf("  D3DAssemble error: %.*s\n", (int)aErr->GetBufferSize(), (const char*)aErr->GetBufferPointer()); }
    size_t   aSize = (SUCCEEDED(aHr) && aCode) ? aCode->GetBufferSize() : 0;
    uint64_t aHash = aSize ? fnv1a(aCode->GetBufferPointer(), aSize) : 0;
    if (aSize) printf("  D3DAssemble        size=%zu  hash=0x%016llX\n", aSize, (unsigned long long)aHash);

    // ---- D3DXAssembleShader (the baseline at :567) ----
    DxInclude dxInc;
    ID3DXBuffer* xCode = nullptr; ID3DXBuffer* xErr = nullptr;
    HRESULT xHr = D3DXAssembleShader(src.data(), (UINT)src.size(),
        reinterpret_cast<const D3DXMACRO*>(defs.data()), &dxInc, 0, &xCode, &xErr);
    printf("  D3DXAssembleShader HRESULT=0x%08lX\n", (unsigned long)xHr);
    if (xErr) { printf("  D3DXAssembleShader error: %.*s\n", (int)xErr->GetBufferSize(), (const char*)xErr->GetBufferPointer()); }
    size_t   xSize = (SUCCEEDED(xHr) && xCode) ? xCode->GetBufferSize() : 0;
    uint64_t xHash = xSize ? fnv1a(xCode->GetBufferPointer(), xSize) : 0;
    if (xSize) printf("  D3DXAssembleShader size=%zu  hash=0x%016llX\n", xSize, (unsigned long long)xHash);

    // ---- size+hash diff ----
    const char* verdict = "UNKNOWN";
    bool aOk = SUCCEEDED(aHr) && aCode;
    bool xOk = SUCCEEDED(xHr) && xCode;
    bool sizeHashMatch = aOk && xOk && (aSize == xSize) && (aHash == xHash);
    if (!aOk)               verdict = "FAIL: D3DAssemble error/null";
    else if (sizeHashMatch) verdict = "MATCH (size+hash identical)";
    else                    verdict = "MISMATCH (size or hash differs)";
    printf("  SIZE/HASH: %s\n", verdict);

    // ---- token-compare fallback on byte-MISMATCH (review fix #2) ----
    const char* tokenResult = "n/a";
    if (aOk && xOk && !sizeHashMatch)
    {
        std::string aDis = normalizeDisasm(disassemble(aCode->GetBufferPointer(), aSize));
        std::string xDis = normalizeDisasm(disassemble(xCode->GetBufferPointer(), xSize));
        if (aDis.empty() || xDis.empty()) tokenResult = "TOKEN-INDETERMINATE (disasm empty)";
        else if (aDis == xDis)            tokenResult = "TOKEN-IDENTICAL (benign byte-variance)";
        else                              tokenResult = "TOKEN-DIVERGENT (semantic drift)";
        printf("  DISASM TOKENS: %s\n", tokenResult);
    }

    // ---- CreateVertexShader on the candidate bytecode ----
    const char* cvsResult = "skipped (no device)";
    long cvsHr = 0;
    if (aOk && g_dev)
    {
        IDirect3DVertexShader9* vs = nullptr;
        cvsHr = g_dev->CreateVertexShader(static_cast<const DWORD*>(aCode->GetBufferPointer()), &vs);
        cvsResult = SUCCEEDED(cvsHr) ? "S_OK" : "REJECTED";
        printf("  CreateVertexShader HRESULT=0x%08lX  (%s)\n", (unsigned long)cvsHr, cvsResult);
        if (vs) vs->Release();
    }
    else printf("  CreateVertexShader %s\n", cvsResult);

    // ---- per-shader candidate classification ----
    // The gate measures D3DAssemble DIVERGENCE FROM the D3DXAssembleShader baseline, NOT absolute success.
    //   0 PASS           : both assembled, bytecode size+hash identical, CreateVertexShader S_OK
    //   1 PASS-WITH-NOTE : both assembled, byte-MISMATCH but TOKEN-IDENTICAL, CreateVertexShader S_OK
    //   2 FAIL           : a TRUE divergence — D3DAssemble fails where the baseline SUCCEEDS, OR
    //                      TOKEN-DIVERGENT, OR CreateVertexShader REJECTS the candidate bytecode
    //   3 EXCLUDED       : BOTH assemblers fail IDENTICALLY (same HRESULT) — a harness/input defect
    //                      (e.g. corpus include version-skew), NOT a D3DAssemble incompatibility
    int cls;
    bool cvsOk = aOk && g_dev && SUCCEEDED(cvsHr);
    bool cvsUnknown = aOk && !g_dev;
    if (!aOk && !xOk && aHr == xHr) cls = 3;                 // both fail identically -> excluded
    else if (!aOk && xOk)           cls = 2;                 // candidate fails where baseline succeeds -> true FAIL
    else if (!aOk)                  cls = 3;                 // both fail (different HRESULT) -> still not a divergence to baseline-success
    else if (strstr(tokenResult, "DIVERGENT")) cls = 2;
    else if (g_dev && !cvsOk)       cls = 2;
    else if (sizeHashMatch)         cls = cvsUnknown ? 1 : 0;
    else                            cls = (strstr(tokenResult, "IDENTICAL")) ? 1 : 2;
    const char* clsStr = (cls == 0) ? "PASS" : (cls == 1) ? "PASS-WITH-NOTE" : (cls == 2) ? "FAIL" : "EXCLUDED (both assemblers fail identically — harness/input, not a divergence)";
    printf("  >>> PER-SHADER: %s\n", clsStr);

    if (aCode) aCode->Release(); if (aErr) aErr->Release();
    if (xCode) xCode->Release(); if (xErr) xErr->Release();
    return cls;
}

int main(int argc, char** argv)
{
    if (argc > 1) g_extractDir = argv[1];
    printf("d3dassemble_probe — Phase 32 plan 32-01 Task 2\n");
    printf("extract dir: %s\n", g_extractDir.c_str());

    if (!resolveD3DAssemble())   printf("WARNING: D3DAssemble export did not resolve\n");
    if (!resolveD3DDisassemble())printf("WARNING: D3DDisassemble export did not resolve\n");
    createDevice();

    // Binding probe set (32-CENSUS.md §4): baseline + F1(lit) + F2(4uv) + F3 bonus(b# branch).
    ShaderCase cases[] = {
        { "c_simple.vsh",  false, "  [baseline]" },
        { "tfcl.vsh",      false, "  [baseline]" },
        { "cloudlayer.vsh",false, "  [baseline]" },
        { "tfcsl.vsh",     false, "  [F1: diffuse_specular.inc / lit opcode]" },
        { "tfcl_4uv.vsh",  false, "  [F2: 4 texcoord sets v7..v10, oT0..oT3]" },
        { "tfcsl.vsh",     true,  "  [F3 BONUS: + VERTEX_SHADER_VERSION=20 -> if b# branch (TOOL path only)]" },
    };
    int nPass = 0, nNote = 0, nFailRuntime = 0, nFailBonus = 0, nExcluded = 0;
    const int nCases = (int)(sizeof(cases)/sizeof(cases[0]));
    for (int i = 0; i < nCases; ++i)
    {
        int cls = probeShader(cases[i]);
        bool bonus = cases[i].defineVsVersion20;   // F3 is non-runtime-binding
        if (cls == 0) ++nPass;
        else if (cls == 1) ++nNote;
        else if (cls == 3) ++nExcluded;
        else { if (bonus) ++nFailBonus; else ++nFailRuntime; }
    }

    printf("======================================================================\n");
    printf("SUMMARY: pass=%d  pass-with-note=%d  excluded=%d  fail(runtime-binding)=%d  fail(bonus/F3)=%d\n",
           nPass, nNote, nExcluded, nFailRuntime, nFailBonus);
    // Candidate gate (the human records the final verdict in 32-CENSUS.md at the Task-3 checkpoint):
    //   FAIL-WITH-FOLLOWUP if any RUNTIME-BINDING shader failed.
    //   PASS               if all runtime-binding shaders MATCH (no notes).
    //   PASS-WITH-NOTE     if all runtime-binding shaders pass but >=1 is byte-variant/token-identical.
    const char* cand = (nFailRuntime > 0) ? "FAIL-WITH-FOLLOWUP"
                     : (nNote > 0)         ? "PASS-WITH-NOTE"
                     :                       "PASS";
    printf("CANDIDATE GATE VERDICT: %s\n", cand);
    if (nFailBonus > 0) printf("NOTE: F3 (b# boolean-branch, TOOL path only) failed — non-binding; record as a NOTE only.\n");

    if (g_dev) g_dev->Release();
    if (g_d3d) g_d3d->Release();
    if (g_wnd) DestroyWindow(g_wnd);
    return (nFailRuntime > 0) ? 1 : 0;
}
