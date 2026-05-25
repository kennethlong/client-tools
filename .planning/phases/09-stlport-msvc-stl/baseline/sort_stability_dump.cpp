// Phase 9 Wave 0 — sort stability baseline.
// Pin a known input array, sort indices via CrcLowerString::LessAbcOrderReferenceComparator,
// dump the resulting strings in sorted order. Acceptance: pre/post Phase 9 dumps match.
//
// Note: LessAbcOrderReferenceComparator::operator()(const CrcLowerString&, const CrcLowerString&)
// takes references — sorting an indirection vector keeps CrcLowerString instances in place
// (avoids copy/move under the MemoryBlockManager).
//
// Output path is taken as an explicit argv (rather than stdout) because the
// MemoryManager debug tracker writes to stdout when allocators tear down.

#include "FirstValidationHelpers.h"

#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedThread/SetupSharedThread.h"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <vector>

static int   s_argc;
static char **s_argv;

static void run()
{
	if (s_argc < 2)
	{
		std::fprintf(stderr, "usage: validation_helper_sort_stability_dump <output-file>\n");
		return;
	}

	const char * const inputs[] = {
		"appearance/wpn_kashyyykian_axe.apt",
		"creature/badge.iff",
		"terrain/tatooine.trn",
		"misc/quest_task_data.iff",
		"string/en/quest/ground/lok_quest_text.stf",
		"appearance/lair_kashyyykian.apt",
		"appearance/wpn_blaster.apt",
		"creature/dewback.iff",
		"creature/bantha.iff",
		"object/creature/player/human_male.iff",
		"object/tangible/component/weapon/component_weapon_blaster_pistol.iff",
		"object/tangible/loot/loot_collection_lava_skull.iff",
	};
	const int N = sizeof(inputs) / sizeof(inputs[0]);

	std::vector<CrcLowerString *> arr;
	arr.reserve(N);
	for (int i = 0; i < N; ++i)
		arr.push_back(new CrcLowerString(inputs[i]));

	std::vector<int> idx(N);
	std::iota(idx.begin(), idx.end(), 0);

	const CrcLowerString::LessAbcOrderReferenceComparator cmp;
	std::sort(idx.begin(), idx.end(), [&arr, &cmp](int a, int b) {
		return cmp(*arr[a], *arr[b]);
	});

	FILE *out = std::fopen(s_argv[1], "wb");
	if (!out)
	{
		std::fprintf(stderr, "cannot open output: %s\n", s_argv[1]);
		for (CrcLowerString *p : arr) delete p;
		return;
	}
	for (int i : idx)
		std::fprintf(out, "%s\n", arr[i]->getString());
	std::fclose(out);

	for (CrcLowerString *p : arr)
		delete p;
}

int main(int argc, char **argv)
{
	SetupSharedThread::install();
	SetupSharedDebug::install(4096);
	{
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_console);
		data.argc = argc;
		data.argv = argv;
		data.demoMode = true;
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
