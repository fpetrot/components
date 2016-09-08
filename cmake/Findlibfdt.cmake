#.rst:
# Findlibfdt
# -----------
#
# Try to find libfdt

find_path(LIBFDT_INCLUDE_DIR libfdt.h PATH_SUFFIXES include)

if (NOT LIBFDT_LIBRARIES)
	find_library(LIBFDT_LIBRARY_RELEASE NAMES fdt ${_LIBFDT_PATHS} PATH_SUFFIXES lib)
	include(SelectLibraryConfigurations)
	SELECT_LIBRARY_CONFIGURATIONS(LIBFDT)
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBFDT
	                          REQUIRED_VARS LIBFDT_LIBRARIES LIBFDT_INCLUDE_DIR)

