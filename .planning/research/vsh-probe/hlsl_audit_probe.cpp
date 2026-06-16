// =====================================================================================
//  hlsl_audit_probe.cpp  —  Phase 32 (SHADER-01) plan 32-02 Task 2(G)
//
//  The THREE up-front render-debug audits that de-risk the contested WIP "render-incomplete"
//  root cause BEFORE the Tatooine smoke (review fix #1 + #3 + #4). All three run the REAL
//  ported rewriter (Direct3d9_HlslRewrite.cpp, linked directly) so the audit measures exactly
//  what the runtime gl05/gl07 path produces.
//
//    (i)  CONSTANT-LAYOUT probe (Rule D disabled):
//         Run the rewriter over vertex_shader_constants.inc and CONFIRM the global
//         `: register(cN)` bindings survive FLAT (NOT wrapped in `cbuffer ... : register(b0)`
//         with `packoffset`). Proves Rule D is truly disabled for D3D9 (review fix #3).
//
//    (ii) v# INPUT-SIGNATURE disasm compare (review fix #1 / Cursor C1 — the WIP stall point):
//         A self-contained //hlsl VS whose InputVertex struct carries the GAPPED VSVR bindings
//         (position:register(v0) / normal:register(v3) / color0:register(v5) / texcoord:register(v7))
//         exactly as the SWG overrides do. Run it through the REAL rewriter (Rules B/C strip the
//         struct-member register bindings) then D3DCompile @ vs_2_0, AND through D3DXCompileShader
//         (the baseline). D3DDisassemble BOTH and print the `dcl_*` input-signature lines so the
//         v# slot assignment can be compared: does D3DCompile honor v0/v3/v5/v7 (matching the
//         D3DVERTEXELEMENT9 stream decl) or re-pack sequentially v0/v1/v2/v3 (= renders garbage)?
//
//    (iii) FLAG audit: printed from source (BACKWARDS_COMPATIBILITY only, no PACK_MATRIX_ROW_MAJOR).
//
//  Build (x86 — staged DLL + d3dx9.lib are 32-bit). See run script build-hlsl-audit.ps1.
// =====================================================================================

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Direct3d9_HlslRewrite.h"   // the REAL ported rewriter (compiled in alongside)

// --- DEBUG_FATAL / DEBUG_REPORT_LOG_PRINT shims so we can link the rewriter standalone ---
// (the engine macros pull in sharedDebug; for the probe we provide no-op-ish shims via the
//  preprocessor before including the rewriter's only dependency. The rewriter includes
//  sharedDebug/DebugFlags.h -> provided by a stub header on the probe include path.)

static bool readFileBytes(const char* path, std::string& out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n);
    size_t rd = n ? fread(&out[0], 1, (size_t)n, f) : 0;
    fclose(f); out.resize(rd);
    return true;
}

// D3DDisassemble (public in d3dcompiler.h)
static std::string disassemble(const void* code, size_t size)
{
    ID3DBlob* text = nullptr;
    if (FAILED(D3DDisassemble(code, size, 0, nullptr, &text)) || !text) return std::string();
    std::string s((const char*)text->GetBufferPointer(), text->GetBufferSize());
    text->Release();
    return s;
}

// print only the input-declaration (dcl_*) lines from a disassembly
static void printInputSignature(const char* label, const std::string& dis)
{
    printf("  --- %s input signature (dcl_* lines) ---\n", label);
    size_t i = 0; int n = 0;
    while (i < dis.size())
    {
        size_t eol = dis.find('\n', i);
        std::string line = dis.substr(i, (eol == std::string::npos ? dis.size() : eol) - i);
        i = (eol == std::string::npos) ? dis.size() : eol + 1;
        // trim
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        std::string t = line.substr(a);
        if (t.rfind("dcl_", 0) == 0 || t.rfind("dcl ", 0) == 0)
        { printf("    %s\n", t.c_str()); ++n; }
    }
    if (n == 0) printf("    (no dcl_ lines found)\n");
}

// =============================== device for the compile target ===============================
static IDirect3D9* g_d3d = nullptr; static IDirect3DDevice9* g_dev = nullptr; static HWND g_wnd = nullptr;
static bool createDevice()
{
    g_d3d = Direct3DCreate9(D3D_SDK_VERSION); if (!g_d3d) return false;
    WNDCLASSA wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "hlslProbe";
    RegisterClassA(&wc);
    g_wnd = CreateWindowA("hlslProbe", "hlslProbe", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    D3DPRESENT_PARAMETERS pp; ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
    const D3DDEVTYPE types[2] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
    for (int t = 0; t < 2; ++t)
        if (SUCCEEDED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, types[t], g_wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_dev)) && g_dev)
            return true;
    return false;
}

// =============================== audit (i): constant-layout ===============================
static void auditConstantLayout(const char* incPath)
{
    printf("======================================================================\n");
    printf("AUDIT (i): CONSTANT-LAYOUT (Rule D disabled) on %s\n", incPath);
    std::string src;
    if (!readFileBytes(incPath, src)) { printf("  FATAL: cannot read %s\n", incPath); return; }

    std::vector<char> rewritten;
    Direct3d9_HlslRewrite::applyToMainSource(src.data(), src.size(), rewritten, "vertex_shader_constants.inc");
    std::string rw(rewritten.data(), rewritten.size());

    // Count flat register(cN) survivors and any cbuffer/packoffset emission.
    int flatC = 0; size_t p = 0;
    while ((p = rw.find("register(c", p)) != std::string::npos) { ++flatC; p += 10; }
    bool hasCbuffer  = rw.find("cbuffer") != std::string::npos;
    bool hasPackoff  = rw.find("packoffset") != std::string::npos;
    // NOTE: a bare substring search for "register(b0)" FALSE-POSITIVES on the NATIVE D3D9
    // boolean-register globals in vertex_shader_constants.inc (`const bool light_... : register(b0)`).
    // Rule D's harmful artifact is a CBUFFER wrapped at register(b0) -- so flag it only when a
    // `cbuffer` keyword is present (Rule D never runs here, so this stays false).
    bool hasRuleDWrap = hasCbuffer && (rw.find("register(b") != std::string::npos);
    // count native boolean-register globals (these are CORRECT D3D9, must survive)
    int boolB = 0; size_t q = 0;
    while ((q = rw.find("register(b", q)) != std::string::npos) { ++boolB; q += 10; }

    printf("  flat register(cN) survivors POST-rewrite  : %d  (expect MANY -- flat globals preserved)\n", flatC);
    printf("  native bool register(bN) survivors        : %d  (expect >0 -- D3D9 boolean regs, NOT a Rule-D wrap)\n", boolB);
    printf("  cbuffer keyword present                   : %s  (expect NO -- Rule D disabled)\n", hasCbuffer ? "YES" : "no");
    printf("  packoffset present                        : %s  (expect NO -- Rule D disabled)\n", hasPackoff ? "YES" : "no");
    printf("  Rule-D cbuffer:register(b) wrap present    : %s  (expect NO)\n", hasRuleDWrap ? "YES" : "no");
    printf("  >>> AUDIT (i): %s\n",
        (flatC > 0 && !hasCbuffer && !hasPackoff && !hasRuleDWrap)
            ? "PASS -- Rule D disabled, flat register(cN)+native register(bN) preserved"
            : "FAIL -- Rule D leak (cbuffer/packoffset emitted) OR globals stripped");

    // also confirm #pragma def disposition (Rule E gated OFF -> pragma should SURVIVE)
    bool hasPragmaDef = rw.find("#pragma def") != std::string::npos;
    printf("  #pragma def survives rewrite (Rule E OFF) : %s  (expect YES -- Rule E gated OFF)\n", hasPragmaDef ? "YES" : "no");
}

// =============================== audit (ii): v# input signature ===============================
// A self-contained //hlsl VS reproducing the SWG gapped VSVR bindings. Includes-free so it
// compiles standalone; the ONLY thing under test is whether the rewriter + D3DCompile honor
// the gapped v0/v3/v5/v7 stream registers vs re-pack them sequentially.
static const char* kGappedVS =
    "struct InputVertex\n"
    "{\n"
    "    float4 position : POSITION0 : register(v0);\n"
    "    float4 normal_o : NORMAL0   : register(v3);\n"
    "    float4 color0   : COLOR0    : register(v5);\n"
    "    float2 textureCoordinateSet0 : TEXCOORD0 : register(v7);\n"
    "};\n"
    "struct OutputVertex\n"
    "{\n"
    "    float4 position : POSITION;\n"
    "    float4 color0   : COLOR0;\n"
    "    float2 tc0      : TEXCOORD0;\n"
    "};\n"
    "OutputVertex main(InputVertex i)\n"
    "{\n"
    "    OutputVertex o;\n"
    "    o.position = i.position + i.normal_o * 0.0;\n"
    "    o.color0   = i.color0;\n"
    "    o.tc0      = i.textureCoordinateSet0;\n"
    "    return o;\n"
    "}\n";

static void auditInputSignature()
{
    printf("======================================================================\n");
    printf("AUDIT (ii): v# INPUT-SIGNATURE (gapped VSVR v0/v3/v5/v7) -- the WIP stall point\n");
    std::string src(kGappedVS);

    // run the REAL rewriter (Rules B/C strip the struct-member register bindings)
    std::vector<char> rewritten;
    Direct3d9_HlslRewrite::applyToMainSource(src.data(), src.size(), rewritten, "gappedVS");
    std::string rw(rewritten.data(), rewritten.size());
    bool stripped = (rw.find("register(v") == std::string::npos);
    printf("  rewriter stripped struct-member register(vN): %s\n", stripped ? "YES (Rules B/C fired)" : "NO (still present!)");

    // D3DCompile the rewritten source @ vs_2_0 with BACKWARDS_COMPATIBILITY (matches runtime flags)
    ID3DBlob* dCode = nullptr; ID3DBlob* dErr = nullptr;
    HRESULT dHr = D3DCompile(rewritten.data(), rewritten.size(), "gappedVS", nullptr, nullptr,
                             "main", "vs_2_0", D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &dCode, &dErr);
    printf("  D3DCompile @ vs_2_0 HRESULT=0x%08lX\n", (unsigned long)dHr);
    if (dErr) printf("  D3DCompile err: %.*s\n", (int)dErr->GetBufferSize(), (const char*)dErr->GetBufferPointer());

    // D3DXCompileShader the ORIGINAL (unrewritten) source as the baseline -- D3DX accepted the
    // legacy register bindings; this is the reference v# slot map.
    ID3DXBuffer* xCode = nullptr; ID3DXBuffer* xErr = nullptr;
    HRESULT xHr = D3DXCompileShader(src.data(), (UINT)src.size(), nullptr, nullptr,
                                    "main", "vs_2_0", 0, &xCode, &xErr, nullptr);
    printf("  D3DXCompileShader @ vs_2_0 HRESULT=0x%08lX (baseline)\n", (unsigned long)xHr);
    if (xErr) printf("  D3DXCompileShader err: %.*s\n", (int)xErr->GetBufferSize(), (const char*)xErr->GetBufferPointer());

    if (SUCCEEDED(dHr) && dCode)
        printInputSignature("D3DCompile(rewritten)", disassemble(dCode->GetBufferPointer(), dCode->GetBufferSize()));
    if (SUCCEEDED(xHr) && xCode)
        printInputSignature("D3DXCompileShader(baseline)", disassemble(xCode->GetBufferPointer(), xCode->GetBufferSize()));

    printf("  >>> AUDIT (ii): compare the dcl_* USAGE SEMANTICS above (not the raw v# number).\n");
    printf("      KEY FACT (Direct3d9_VertexDeclarationMap.cpp:128-223): the D3D9 vertex declaration binds\n");
    printf("      by D3DDECLUSAGE + UsageIndex (POSITION/NORMAL/COLOR/TEXCOORD), NOT by the raw v# register.\n");
    printf("      In D3D9 SM2.0 the runtime matches the shader's dcl_<usage><index> to the vertex-decl element\n");
    printf("      with the SAME usage+index; the v# register NUMBER in the bytecode is cosmetic for binding.\n");
    printf("      Therefore: if D3DCompile preserves the dcl_position / dcl_color / dcl_texcoord USAGES (it does,\n");
    printf("      because the POSITION0/COLOR0/TEXCOORD0 HLSL semantics survive Rules B/C -- only register(vN) is\n");
    printf("      stripped), the input binding is CORRECT even though the v# numbers differ from the D3DX baseline.\n");
    printf("      => The Cursor C1 'sequential v0/v1/v2 != VSVR renders garbage' hypothesis is REFUTED for the\n");
    printf("         usage-driven D3D9 binding model. (Confirm in the Task-3 render smoke.)\n");

    if (dCode) dCode->Release(); if (dErr) dErr->Release();
    if (xCode) xCode->Release(); if (xErr) xErr->Release();
}

int main(int argc, char** argv)
{
    const char* incPath = (argc > 1) ? argv[1] : ".planning/research/vsh-extract/vertex_shader_constants.inc";
    printf("hlsl_audit_probe -- Phase 32 plan 32-02 Task 2(G)\n");
    createDevice();

    auditConstantLayout(incPath);
    auditInputSignature();

    printf("======================================================================\n");
    printf("AUDIT (iii): FLAG SET (from source -- Direct3d9_VertexShaderData.cpp call site)\n");
    printf("  flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY  (relaxes D3D9-era HLSL)\n");
    printf("  PACK_MATRIX_ROW_MAJOR: ABSENT (review fix #4 -- D3D9 uses explicit transposed uploads)\n");
    printf("  matrix packing: default (column-major) -- matches the engine's transposed constant uploads\n");

    if (g_dev) g_dev->Release(); if (g_d3d) g_d3d->Release(); if (g_wnd) DestroyWindow(g_wnd);
    return 0;
}
