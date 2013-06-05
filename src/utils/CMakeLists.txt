# Build library utils.
ADD_LIBRARY(utils md5_hash.cc simple_hash.cc string_codec.cc split_string.cc string_printf.cc string_util.cc)

# Build unittests.
SET(LIBS utils base pthread)

# Install library and header files
INSTALL(TARGETS utils DESTINATION lib/FastDCS)
FILE(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${HEADER_FILES} DESTINATION include/FastDCS/utils)
