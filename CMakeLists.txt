cmake_minimum_required(VERSION 3.16.0)
project(zita-njbridge VERSION 0.4.4)

add_definitions(-DVERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(CMAKE_CPPFLAGS_FLAGS "${CMAKE_CPPFLAGS_FLAGS} -I. -MMD -MP")
set(CMAKE_CXXFLAGS_FLAGS "${CMAKE_CXXFLAGS_FLAGS} -O2 -Wall")
include(GNUInstallDirs)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Dependencies
find_package(Threads REQUIRED)
find_package(Jack REQUIRED)
add_subdirectory(zita-resampler)
if (TARGET zita-resampler)
    add_library(zita-resampler::zita-resampler ALIAS zita-resampler)
else ()
    find_package(zita-resampler CONFIG REQUIRED)
endif ()

# Source control
set(J2N_SOURCES ${PROJECT_SOURCE_DIR}/source/netdata.cc
        ${PROJECT_SOURCE_DIR}/source/jacktx.cc
        ${PROJECT_SOURCE_DIR}/source/nettx.cc
        ${PROJECT_SOURCE_DIR}/source/pxthread.cc
        ${PROJECT_SOURCE_DIR}/source/lfqueue.cc
        ${PROJECT_SOURCE_DIR}/source/zsockets.cc)

set(N2J_SOURCES ${PROJECT_SOURCE_DIR}/source/zita-n2j.cc
        ${PROJECT_SOURCE_DIR}/source/netdata.cc
        ${PROJECT_SOURCE_DIR}/source/jackrx.cc
        ${PROJECT_SOURCE_DIR}/source/netrx.cc
        ${PROJECT_SOURCE_DIR}/source/pxthread.cc
        ${PROJECT_SOURCE_DIR}/source/lfqueue.cc
        ${PROJECT_SOURCE_DIR}/source/zsockets.cc
        ${PROJECT_SOURCE_DIR}/source/syncrx.cc)

add_executable(zita-j2n ${PROJECT_SOURCE_DIR}/source/zita-j2n.cc ${J2N_SOURCES})
add_executable(zita-n2j ${PROJECT_SOURCE_DIR}/source/zita-n2j.cc ${N2J_SOURCES})
target_include_directories(zita-j2n
        PUBLIC
        ${JACK_INCLUDE_DIRS}
        )
target_link_libraries(zita-j2n
        PRIVATE
        Threads::Threads
        PUBLIC
        ${JACK_LIBRARIES}
        )
target_include_directories(zita-n2j
        PRIVATE
        zita-resampler/source
        ${JACK_INCLUDE_DIRS}
        )
target_link_libraries(zita-n2j
        PRIVATE
        Threads::Threads
        PUBLIC
        zita-resampler::zita-resampler
        ${JACK_LIBRARIES}
        )

install(TARGETS zita-j2n zita-n2j DESTINATION bin)