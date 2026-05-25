# Phase 9 Wave 0 — validation helpers.
# OFF by default so a regular build remains bit-identical with these absent.
# Enable with: cmake -S src -B build -A Win32 -DWHITENGOLD_BUILD_VALIDATION_HELPERS=ON
# Pattern: src/engine/shared/application/LabelHashTool/CMakeLists.txt

if(NOT WHITENGOLD_BUILD_VALIDATION_HELPERS)
	return()
endif()

set(BASELINE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Shared include set used by all three helpers.
set(VH_INCLUDES
	${BASELINE_DIR}
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedCompression/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedThread/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedSynchronization/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedLog/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMessageDispatch/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetworkMessages/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedUtility/include/public
	${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFile/include/public
	${SWG_OURS}/localization/include
	${SWG_OURS}/unicode/include
	${SWG_OURS}/unicodeArchive/include/public
	${SWG_OURS}/archive/include
	${SWG_OURS}/fileInterface/include/public
)

# Shared link set — mirrors LabelHashTool plus sharedFile/sharedUtility for the IFF helper.
set(VH_LIBS
	sharedFoundation
	sharedDebug
	sharedLog
	sharedNetworkMessages
	sharedMessageDispatch
	sharedUtility
	sharedCompression
	sharedFile
	sharedThread
	sharedMemoryManager
	sharedSynchronization
	localization
	localizationArchive
	unicode
	unicodeArchive
	archive
	fileInterface
	${STLPORT_LIBRARY}
	legacy_stdio_definitions
)

# Shared PCH source.
set(VH_PCH_SOURCES
	${BASELINE_DIR}/FirstValidationHelpers.cpp
	${BASELINE_DIR}/FirstValidationHelpers.h
)

# --- helper 1: std::hash dump --------------------------------------------------
add_executable(validation_helper_std_hash_dump
	${VH_PCH_SOURCES}
	${BASELINE_DIR}/std_hash_dump.cpp
	$<TARGET_OBJECTS:stlport_vc143_compat>
)
target_include_directories(validation_helper_std_hash_dump PRIVATE ${VH_INCLUDES})
target_link_libraries(validation_helper_std_hash_dump PRIVATE ${VH_LIBS})
set_target_properties(validation_helper_std_hash_dump PROPERTIES
	LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"
	LINK_FLAGS_RELEASE "/FORCE:MULTIPLE"
)
target_precompile_headers(validation_helper_std_hash_dump PRIVATE
	${BASELINE_DIR}/FirstValidationHelpers.h
)

# --- helper 2: DataTable round-trip --------------------------------------------
add_executable(validation_helper_datatable_round_trip
	${VH_PCH_SOURCES}
	${BASELINE_DIR}/datatable_round_trip.cpp
	$<TARGET_OBJECTS:stlport_vc143_compat>
)
target_include_directories(validation_helper_datatable_round_trip PRIVATE ${VH_INCLUDES})
target_link_libraries(validation_helper_datatable_round_trip PRIVATE ${VH_LIBS})
set_target_properties(validation_helper_datatable_round_trip PROPERTIES
	LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"
	LINK_FLAGS_RELEASE "/FORCE:MULTIPLE"
)
target_precompile_headers(validation_helper_datatable_round_trip PRIVATE
	${BASELINE_DIR}/FirstValidationHelpers.h
)

# --- helper 3: sort stability --------------------------------------------------
add_executable(validation_helper_sort_stability_dump
	${VH_PCH_SOURCES}
	${BASELINE_DIR}/sort_stability_dump.cpp
	$<TARGET_OBJECTS:stlport_vc143_compat>
)
target_include_directories(validation_helper_sort_stability_dump PRIVATE ${VH_INCLUDES})
target_link_libraries(validation_helper_sort_stability_dump PRIVATE ${VH_LIBS})
set_target_properties(validation_helper_sort_stability_dump PROPERTIES
	LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"
	LINK_FLAGS_RELEASE "/FORCE:MULTIPLE"
)
target_precompile_headers(validation_helper_sort_stability_dump PRIVATE
	${BASELINE_DIR}/FirstValidationHelpers.h
)
