##############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2021 Liberty Global Service BV
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##############################################################################

cmake_minimum_required(VERSION 3.0)

project(rpcserver VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCE_FILES
    include/IAbstractRpcServer.h
    include/IRpcExternalEventListener.h
    include/IRpcMethodBinder.h
    include/IRpcMethodCaller.h
    include/IRpcServerBuilder.h
    include/IRpcServerControl.h
    include/WsRpcServerBuilder.h
    include/WsRpcServerBuilderExtended.h
    src/connector/AbstractPeerServerConnector.h
    src/connector/PeerProtocolHandler.h
    src/connector/WsClientCtx.h
    src/connector/WsConnector.h
    src/interface/IClientPeerConnectionHandler.h
    src/interface/IPeerServerConnector.h
    src/interface/IPeerServerHandler.h
    src/interface/IRpcEventManagerListener.h
    src/server/RpcEventManager.h
    src/server/RpcPeerServer.h
    src/WsRpcServer.h
    src/WsRpcServer.cpp
    src/WsRpcServerBuilder.cpp
    src/WsRpcServerBuilderExtended.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1)

SET_PROPERTY(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script='${CMAKE_CURRENT_SOURCE_DIR}/export_symbols'")
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/export_symbols")

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

find_package(Boost REQUIRED)
find_package(websocketpp REQUIRED)

find_package(PkgConfig)
pkg_search_module(JSONCPP           REQUIRED "jsoncpp")
pkg_search_module(JSONRPCCPP_CLIENT REQUIRED "libjsonrpccpp-client")
pkg_search_module(JSONRPCCPP_SERVER REQUIRED "libjsonrpccpp-server")

target_compile_options(${PROJECT_NAME} PRIVATE
                ${JSONCPP_CFLAGS_OTHER}
                ${JSONRPCCPP_CLIENT_CFLAGS_OTHER}
                ${JSONRPCCPP_SERVER_CFLAGS_OTHER}
)

target_include_directories(${PROJECT_NAME} PRIVATE
                ${Boost_INCLUDE_DIRS}
                ${WEBSOCKETPP_INCLUDE_DIR}
                ${JSONCPP_INCLUDE_DIRS}
                ${JSONRPCCPP_CLIENT_INCLUDE_DIRS}
                ${JSONRPCCPP_SERVER_INCLUDE_DIRS}
)

link_directories(
                ${JSONCPP_LIBRARY_DIRS}
                ${JSONRPCCPP_CLIENT_LIBRARY_DIRS}
                ${JSONRPCCPP_SERVER_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
                ${JSONCPP_LIBRARIES}
                ${JSONRPCCPP_CLIENT_LIBRARIES}
                ${JSONRPCCPP_SERVER_LIBRARIES}
)

set(PACKAGE_NAME ${PROJECT_NAME})

# NOTE: before using ${CMAKE_INSTALL_LIBDIR} or other such variables make sure that all platforms
# have https://git.openembedded.org/openembedded-core/commit/meta/classes/cmake.bbclass?id=c03b32bd71dbe04f2f239556fea0b53215e403d7

install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} EXPORT ${PACKAGE_NAME}Config
    LIBRARY DESTINATION lib
)

install(EXPORT ${PACKAGE_NAME}Config DESTINATION share/${PACKAGE_NAME}/cmake)

export(TARGETS ${PROJECT_NAME} FILE ${PACKAGE_NAME}Config.cmake)
