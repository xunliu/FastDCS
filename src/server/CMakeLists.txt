# Build protobuf
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS tracker_protocol.proto)

# Build library server.
ADD_LIBRARY(server epoller.cc signaling_queue.cc tcp_socket.cc tracker.cc master.cc worker.cc tracker_protocol.pb.cc enviroment.cc protofile.cc connector.cc tracker_main.cc)

# Build unittests.
set(LIBS server base protobuf utils system libconhash pthread)

# Install library and header files
INSTALL(TARGETS server DESTINATION lib/FastDCS)
FILE(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${HEADER_FILES} DESTINATION include/FastDCS/server)
