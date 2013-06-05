# Build library data.
ADD_LIBRARY(data mysql_accessor.cc)

# Build unittests.
SET(LIBS data utils system base protobuf pthread mysqlclient)

# Install library and header files
INSTALL(TARGETS data DESTINATION lib/FastDCS)
FILE(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${HEADER_FILES} DESTINATION include/FastDCS/data)
