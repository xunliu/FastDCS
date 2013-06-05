# Copyright 2013 FastDCS Inc.
# Author: Liu Xun (my@liuxun.org)
#
# Before you can build FastDCS, you need to build and install
# dependent libraries following
#
#   http://www.FastDCS.com/
#
# This README assumes you installed all 3rd-party libraries into a
# directory named /home/you/3rd-party.  You need to use that directory
# to replace the directory in the following variable setting:
#
#   set(THIRD_PARTY_DIR "/Users/liuxun/3rd-party")
#
# Now, you can build the FastDCS using the following commands:
#
#  $> mkdir build
#  $> cd build
#  $> cmake ..
#  $> make
#
# Thus you check out the FastDCS project and build it in a
# subdirectory "build".  If you want further to install the built
# project, you can modify the default installation directory:
#
#   set(CMAKE_INSTALL_PREFIX "/home/liuxun/FastDCS")
#
# and type the command
#
#  $> make install
#
# If you want to use distcc for a distributed build, substitute above
# command
#   cmake ../FastDCS
# by
#   CC=distcc cmake ../FastDCS
#
PROJECT("FastDCS")

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)  # Requires 2.8 for protobuf support.

#------------------------------------------------------------------------------
# Add protobuf compilation support
#------------------------------------------------------------------------------
INCLUDE("FindProtobuf")
FIND_PACKAGE(Protobuf REQUIRED)
FIND_PACKAGE(Threads REQUIRED)

#------------------------------------------------------------------------------
# Take almost all warnining;
# Take warnings as errors;
# Do not generate debug symbols;
# Optimization level 2;
#------------------------------------------------------------------------------
ADD_DEFINITIONS(" -DDEBUG -Wall -Wno-long-long -Wno-sign-compare -Werror -O2 ")

SET(CMAKE_BUILD_TYPE "Debug") 
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O3 -Wall -pedantic -ansi -g -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#------------------------------------------------------------------------------
# Declare where our project will be installed.
#------------------------------------------------------------------------------
SET(CMAKE_INSTALL_PREFIX "/home/liuxun/FastDCS")

#------------------------------------------------------------------------------
# Declare where the third party libraries were installed.
#------------------------------------------------------------------------------
SET(THIRD_PARTY_DIR "/home/liuxun/3rd-party")

#------------------------------------------------------------------------------
# Declare where the mysql libraries were installed.
#------------------------------------------------------------------------------
SET(MYSQL_DIR "/usr/local/mysql")

#------------------------------------------------------------------------------
# Set include paths.
# Add new lines below if you installed more 3rd-party libs.
#------------------------------------------------------------------------------
INCLUDE_DIRECTORIES(
  "${PROJECT_SOURCE_DIR}";
  "${PROJECT_BINARY_DIR}";
  "${PROTOBUF_INCLUDE_DIRS}";
  "${MYSQL_DIR}/include/mysql";
  )

#------------------------------------------------------------------------------
# Set libray paths.
# Add new lines below if you add new packages in FastDCS project, 
# or installed more 3rd-party libs.
#------------------------------------------------------------------------------
LINK_DIRECTORIES(
  "${PROJECT_BINARY_DIR}/src/base";
  "${PROJECT_BINARY_DIR}/src/data"; 
  "${PROJECT_BINARY_DIR}/src/demo";   
  "${PROJECT_BINARY_DIR}/src/libconhash";
  "${PROJECT_BINARY_DIR}/src/server";  
  "${PROJECT_BINARY_DIR}/src/system";
  "${PROJECT_BINARY_DIR}/src/utils";  
  "${MYSQL_DIR}/lib/mysql";
  )

#------------------------------------------------------------------------------
# Declare packages in paralgo project.
# Add new lines below if you add new packages in FastDCS project.
#------------------------------------------------------------------------------
ADD_SUBDIRECTORY(src/base)
ADD_SUBDIRECTORY(src/data)
ADD_SUBDIRECTORY(src/demo)
ADD_SUBDIRECTORY(src/libconhash)
ADD_SUBDIRECTORY(src/server)
ADD_SUBDIRECTORY(src/system)
ADD_SUBDIRECTORY(src/utils)
