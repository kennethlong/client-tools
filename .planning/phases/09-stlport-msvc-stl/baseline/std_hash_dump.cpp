// Phase 9 Wave 0 — emit CrcLowerString::calculateCrc + std::hash<std::string>
// for a fixed input list. Output: "<input>\t0x<crc8hex>\t<std-hash-decimal>" per line.
//
// Build target: validation_helper_std_hash_dump (gated behind WHITENGOLD_BUILD_VALIDATION_HELPERS).
// Run: validation_helper_std_hash_dump <known-strings.txt> > before-crc.txt
//
// Pattern: src/engine/shared/application/LabelHashTool/src/shared/LabelHashTool.cpp
// Persistent CRC: CrcLowerString::calculateCrc — verified vs Crc.h (no Crc::calculateLowerString
// exists in this codebase; the lower-string variant lives on CrcLowerString).

#include "FirstValidationHelpers.h"

#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedThread/SetupSharedThread.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

static int   s_argc;
static char **s_argv;

// STLPort 4.5.3's basic_filebuf crashes on codecvt->encoding() vtable mismatch
// against the MSVC 2022 runtime — all STLPort fstream I/O silently fails. Use
// C-runtime FILE* to bypass it. (Documented in user memory and the v1
// project_stlport_fstream_crash incident.)
//
// The output is written to a FILE* the helper opens itself rather than stdout
// because MemoryManager debug output interleaves with stdout each time an
// allocator tracker decrements (every std::string going out of scope corrupts
// the dump mid-line).
static void run()
{
	if (s_argc < 3)
	{
		std::fprintf(stderr, "usage: validation_helper_std_hash_dump <input-list> <output-file>\n");
		return;
	}

	FILE *in = std::fopen(s_argv[1], "rb");
	if (!in)
	{
		std::fprintf(stderr, "cannot open input %s\n", s_argv[1]);
		return;
	}

	FILE *out = std::fopen(s_argv[2], "wb");
	if (!out)
	{
		std::fprintf(stderr, "cannot open output %s\n", s_argv[2]);
		std::fclose(in);
		return;
	}

	// One std::string instance reused across iterations to minimise allocator pressure.
	std::hash<std::string> stdHasher;
	std::string scratch;
	scratch.reserve(256);

	char buf[1024];
	while (std::fgets(buf, sizeof(buf), in))
	{
		std::size_t len = std::strlen(buf);
		while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
			buf[--len] = '\0';
		if (len == 0)
			continue;
		scratch.assign(buf, len);
		const uint32      crc = CrcLowerString::calculateCrc(buf);
		const std::size_t sh  = stdHasher(scratch);
		std::fprintf(out, "%s\t0x%08x\t%zu\n", buf, crc, sh);
	}
	std::fclose(in);
	std::fclose(out);
}

int main(int argc, char **argv)
{
	SetupSharedThread::install();
	SetupSharedDebug::install(4096);
	{
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_console);
		SetupSharedFoundation::install(data);
	}
	SetupSharedCompression::install();

	s_argc = argc;
	s_argv = argv;

	SetupSharedFoundation::callbackWithExceptionHandling(run);
	SetupSharedFoundation::remove();
	SetupSharedThread::remove();
	return 0;
}
