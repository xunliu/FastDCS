# Build tools
SET(LIBS server libconhash data utils base system protobuf pthread  mysqlclient)

ADD_EXECUTABLE(dict_master dict_master.cc)
TARGET_LINK_LIBRARIES(dict_master ${LIBS})

ADD_EXECUTABLE(dict_worker dict_worker.cc)
TARGET_LINK_LIBRARIES(dict_worker ${LIBS})

ADD_EXECUTABLE(dict_mock dict_mock.cc)
TARGET_LINK_LIBRARIES(dict_mock ${LIBS})

SET(DEMO_FILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/README
  ${CMAKE_CURRENT_SOURCE_DIR}/dict.sql 
  ${CMAKE_CURRENT_SOURCE_DIR}/dict.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_master_32301.conf
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_master_32302.conf
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_master_32303.conf
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_worker.conf
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_master_32301.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_master_32302.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_master_32303.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/dict_worker.sh
  )
INSTALL(FILES ${DEMO_FILES} DESTINATION demo)

# Install library and header files
INSTALL(TARGETS dict_master DESTINATION demo)
INSTALL(TARGETS dict_worker DESTINATION demo)
INSTALL(TARGETS dict_mock DESTINATION demo)
