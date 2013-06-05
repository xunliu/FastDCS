# Build library base.
ADD_LIBRARY(base config_bst.cc config_file.cc logging.cc skip_list.cc varint32.cc)

# Build unittests.
SET(LIBS base system protobuf pthread)

# Install library and header files
INSTALL(TARGETS base DESTINATION lib/FastDCS)
FILE(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${HEADER_FILES} DESTINATION include/FastDCS/base)
