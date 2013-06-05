# Build library server.
ADD_LIBRARY(libconhash conhash.c conhash_inter.c conhash_util.c md5.c util_rbtree.c)

# Build unittests.
SET(LIBS libconhash pthread)

ADD_EXECUTABLE(sample sample.c)
TARGET_LINK_LIBRARIES(sample ${LIBS})

# Install library and header files
INSTALL(TARGETS libconhash DESTINATION lib/FastDCS)
FILE(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${HEADER_FILES} DESTINATION include/FastDCS/libconhash)
