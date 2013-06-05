# Build library system.
ADD_LIBRARY(system daemon.cc condition_variable.cc)

# Build unittests.
SET(LIBS system base protobuf pthread)

# Install library and header files
INSTALL(TARGETS system DESTINATION lib/FastDCS)
FILE(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${HEADER_FILES} DESTINATION include/FastDCS/system)
