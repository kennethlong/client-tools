// Phase 9 Wave 0 — D-06 round-trip persist baseline.
// Loads a DataTable IFF and dumps every cell as text.
// Format: "<row>\t<col-index>\t<col-name>\t<basic-type>\t<value>" per line.
//
// Build target: validation_helper_datatable_round_trip (gated behind WHITENGOLD_BUILD_VALIDATION_HELPERS).
// Run: validation_helper_datatable_round_trip <abs-path-to-iff> <output-file>
//
// Setup pattern matches src/engine/shared/application/DataTableTool/src/shared/DataTableTool.cpp.
// Plan recommendation was misc/quest_task_data.iff but that file ships only inside .tre archives
// in this user's retail install. Standalone DataTable IFFs live under
// D:/Code/SWGSource Client v3.0/datatables/... — use radial_menu.iff or any other small DataTable.
//
// Output path is taken as an explicit argv (rather than stdout) because the
// MemoryManager debug tracker writes to stdout each time an allocator decrements
// and would corrupt the dump mid-line otherwise.

#include "FirstValidationHelpers.h"

#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/Iff.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/DataTableColumnType.h"
#include "sharedUtility/DataTableManager.h"

#include <cstdio>
#include <string>

static int   s_argc;
static char **s_argv;

static void run()
{
	if (s_argc < 3)
	{
		std::fprintf(stderr, "usage: validation_helper_datatable_round_trip <datatable.iff> <output-file>\n");
		return;
	}

	TreeFile::addSearchAbsolute(0);

	Iff iff;
	if (!iff.open(s_argv[1], true))
	{
		std::fprintf(stderr, "cannot open IFF: %s\n", s_argv[1]);
		return;
	}

	DataTable dt;
	dt.load(iff);

	FILE *out = std::fopen(s_argv[2], "wb");
	if (!out)
	{
		std::fprintf(stderr, "cannot open output: %s\n", s_argv[2]);
		return;
	}

	const int rows = dt.getNumRows();
	const int cols = dt.getNumColumns();
	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < cols; ++col)
		{
			const std::string &cname = dt.getColumnName(col);
			const DataTableColumnType &ctype = dt.getDataTypeForColumn(col);
			const DataTableColumnType::DataType basic = ctype.getBasicType();
			std::fprintf(out, "%d\t%d\t%s\t%d\t", row, col, cname.c_str(), static_cast<int>(basic));
			switch (basic)
			{
				case DataTableColumnType::DT_Int:
					std::fprintf(out, "%d", dt.getIntValue(col, row));
					break;
				case DataTableColumnType::DT_Float:
					// %.6f is deterministic across STL versions; %g is locale-sensitive.
					std::fprintf(out, "%.6f", dt.getFloatValue(col, row));
					break;
				case DataTableColumnType::DT_String:
				{
					const char * const s = dt.getStringValue(col, row);
					if (s)
						std::fprintf(out, "%s", s);
					break;
				}
				default:
					break;
			}
			std::fputc('\n', out);
		}
	}
	std::fclose(out);
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
	SetupSharedFile::install(false);
	DataTableManager::install();

	s_argc = argc;
	s_argv = argv;

	SetupSharedFoundation::callbackWithExceptionHandling(run);
	SetupSharedFoundation::remove();
	SetupSharedThread::remove();
	return 0;
}
