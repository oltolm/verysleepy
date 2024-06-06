cmake_minimum_required(VERSION 3.15)
project(
  verysleepy
  VERSION 0.91.0
  LANGUAGES CXX
)

find_program(CCACHE ccache)

if(CCACHE)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

add_executable(
  sleepy WIN32
  src/profiler/debugger.cpp
  src/profiler/processinfo.cpp
  src/profiler/profiler.cpp
  src/profiler/profilerthread.cpp
  src/profiler/symbolinfo.cpp
  src/profiler/threadinfo.cpp
  src/utils/dbginterface.cpp
  src/utils/mythread.cpp
  src/utils/osutils.cpp
  src/utils/stringutils.cpp
  src/utils/WoW64.cpp
  src/wxProfilerGUI/aboutdlg.cpp
  src/wxProfilerGUI/CallstackView.cpp
  src/wxProfilerGUI/capturewin.cpp
  src/wxProfilerGUI/contextmenu.cpp
  src/wxProfilerGUI/database.cpp
  src/wxProfilerGUI/latesymbolinfo.cpp
  src/wxProfilerGUI/launchdlg.cpp
  src/wxProfilerGUI/logview.cpp
  src/wxProfilerGUI/mainwin.cpp
  src/wxProfilerGUI/optionsdlg.cpp
  src/wxProfilerGUI/processlist.cpp
  src/wxProfilerGUI/proclist.cpp
  src/wxProfilerGUI/profilergui.cpp
  src/wxProfilerGUI/sourceview.cpp
  src/wxProfilerGUI/threadlist.cpp
  src/wxProfilerGUI/threadpicker.cpp
  src/wxProfilerGUI/threadsview.cpp
)

target_sources(sleepy PRIVATE src/res/sleepy.rc)
target_compile_features(sleepy PRIVATE cxx_std_14)

set(wxWidgets_USE_STATIC
  OFF
  CACHE BOOL "Link libraries statically?"
)
find_package(wxWidgets REQUIRED COMPONENTS core base aui propgrid stc)
include(${wxWidgets_USE_FILE})
target_link_libraries(sleepy PRIVATE dbgeng Shlwapi Winmm ${wxWidgets_LIBRARIES})

# target_link_options(sleepy PRIVATE -static -static-libgcc -static-libstdc++)
target_compile_definitions(sleepy PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(sleepy PRIVATE -Wno-missing-field-initializers)

if(MSVC)
  target_link_options(sleepy PRIVATE /MANIFEST:NO)
  target_compile_options(sleepy PRIVATE /MP)
  target_sources(sleepy PRIVATE src/crashback/client/client.cpp)
endif()

execute_process(
  COMMAND git describe --tags --dirty
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

set(GIT_VERSION_PATCH 0)
set(GIT_VERSION_DIRTY 0)
string(REGEX MATCH "^.+-(.*)" version "${GIT_VERSION}")

if(CMAKE_MATCH_1 AND NOT CMAKE_MATCH_1 STREQUAL "dirty")
  set(GIT_VERSION_PATCH "${CMAKE_MATCH_1}")
endif()

string(REGEX MATCH "-dirty$" dirty "${GIT_VERSION}")

if(dirty)
  set(GIT_VERSION_DIRTY 1)
endif()

configure_file("src/version.h.in" "${PROJECT_BINARY_DIR}/src/version.h")

target_include_directories(sleepy PRIVATE "${PROJECT_BINARY_DIR}/src")

if(NOT CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
  target_precompile_headers(
    sleepy
    PRIVATE
    <algorithm>
    <filesystem>
    <iostream>
    <map>
    <wx/wxprec.h>
  )
endif()
