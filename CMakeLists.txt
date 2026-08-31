cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0207)
  cmake_policy(SET CMP0207 NEW)
endif()

project(
  verysleepy
  VERSION 0.91.0
  LANGUAGES CXX
)

set(VERYSLEEPY_APPNAME "Very Sleepy")
set(VERYSLEEPY_VENDOR "Oleg Tolmatcev")
set(VERYSLEEPY_APPURL "https://github.com/oltolm/verysleepy")
set(VERYSLEEPY_GITURL "https://github.com/oltolm/verysleepy")
set(VERYSLEEPY_FORMAT_VERSION "0.92")
set(VERYSLEEPY_BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}" CACHE STRING "Sequential build number")

if(VERYSLEEPY_BUILD_NUMBER STREQUAL "")
  set(VERYSLEEPY_BUILD_NUMBER "0")
endif()

if(NOT VERYSLEEPY_BUILD_NUMBER MATCHES "^[0-9]+$")
  message(FATAL_ERROR "VERYSLEEPY_BUILD_NUMBER must be numeric.")
endif()

math(EXPR VERYSLEEPY_FILE_VERSION_MAJOR "${VERYSLEEPY_BUILD_NUMBER} / 65536")
math(EXPR VERYSLEEPY_FILE_VERSION_MINOR "${VERYSLEEPY_BUILD_NUMBER} % 65536")
set(VERYSLEEPY_FILE_VERSION_PATCH 0)

configure_file("src/appinfo.h.in" "${PROJECT_BINARY_DIR}/src/appinfo.h" @ONLY)

find_program(CCACHE ccache)

if(CCACHE)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

set(VERYSLEEPY_SOURCES
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
  src/wxProfilerGUI/persistentlistctrl.cpp
  src/wxProfilerGUI/processlist.cpp
  src/wxProfilerGUI/proclist.cpp
  src/wxProfilerGUI/profilergui.cpp
  src/wxProfilerGUI/sourceview.cpp
  src/wxProfilerGUI/threadlist.cpp
  src/wxProfilerGUI/threadpicker.cpp
  src/wxProfilerGUI/threadsview.cpp
)

# The same program twice. sleepy.exe is the GUI subsystem build; sleepy.com is the
# console one, which cmd finds first because PATHEXT lists .com before .exe. Being a
# console program is what makes redirection work: it has a console of its own, so
# whatever it profiles inherits that and our standard handles along with it.
add_executable(sleepy WIN32 ${VERYSLEEPY_SOURCES})
add_executable(sleepy_com ${VERYSLEEPY_SOURCES})
set_target_properties(
  sleepy_com PROPERTIES
  OUTPUT_NAME "sleepy"
  SUFFIX ".com"
)

foreach(target sleepy sleepy_com)
  target_sources(${target} PRIVATE src/res/sleepy.rc)
  target_compile_features(${target} PRIVATE cxx_std_14)
endforeach()

set(wxWidgets_USE_STATIC
  OFF
  CACHE BOOL "Link libraries statically?"
)
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base aui propgrid stc)
foreach(target sleepy sleepy_com)
  target_link_libraries(${target} PRIVATE dbgeng Shlwapi Winmm wxWidgets::wxWidgets)
  target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
  target_compile_options(${target} PRIVATE -Wno-missing-field-initializers)
endforeach()

# target_link_options(sleepy PRIVATE -static -static-libgcc -static-libstdc++)

execute_process(
  COMMAND git rev-parse --short HEAD
  OUTPUT_VARIABLE GIT_SHORT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
  RESULT_VARIABLE GIT_REV_PARSE_RESULT
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

set(VERYSLEEPY_VERSION "${VERYSLEEPY_BUILD_NUMBER}")
set(GIT_VERSION_DIRTY 0)

if(GIT_REV_PARSE_RESULT EQUAL 0 AND NOT GIT_SHORT_SHA STREQUAL "")
  if(VERYSLEEPY_BUILD_NUMBER STREQUAL "0")
    set(VERYSLEEPY_VERSION "local-g${GIT_SHORT_SHA}")
  else()
    string(APPEND VERYSLEEPY_VERSION "-g${GIT_SHORT_SHA}")
  endif()
elseif(VERYSLEEPY_BUILD_NUMBER STREQUAL "0")
  set(VERYSLEEPY_VERSION "local")
endif()

execute_process(
  COMMAND git diff --quiet --ignore-cr-at-eol --ignore-submodules=dirty
  RESULT_VARIABLE GIT_DIFF_RESULT
  ERROR_QUIET
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

if(GIT_DIFF_RESULT EQUAL 1)
  set(GIT_VERSION_DIRTY 1)
  string(APPEND VERYSLEEPY_VERSION "-dirty")
endif()

configure_file("src/version.h.in" "${PROJECT_BINARY_DIR}/src/version.h" @ONLY)

foreach(target sleepy sleepy_com)
  target_include_directories(${target} PRIVATE "${PROJECT_BINARY_DIR}/src")
endforeach()

if(NOT CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
  foreach(target sleepy sleepy_com)
    target_precompile_headers(
      ${target}
      PRIVATE
      <algorithm>
      <filesystem>
      <iostream>
      <map>
      <wx/wxprec.h>
    )
  endforeach()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/thirdparty/drmingw/CMakeLists.txt")
  message(FATAL_ERROR "thirdparty/drmingw is missing. Run git submodule update --init --recursive.")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(
  "${PROJECT_SOURCE_DIR}/thirdparty/drmingw"
  "${PROJECT_BINARY_DIR}/thirdparty/drmingw"
  EXCLUDE_FROM_ALL
)

if(TARGET mgwhelp)
  add_dependencies(sleepy sleepy_com mgwhelp)

  # sleepy loads mgwhelp under the name it is shipped as, so stage a copy beside the
  # executable whenever it is rebuilt. The install rule renames it the same way. This
  # is driven by the output file rather than a POST_BUILD on sleepy, because sleepy
  # does not relink when mgwhelp alone changes, and the stale copy left behind then
  # gets loaded instead of the one just built.
  set(VERYSLEEPY_STAGED_MGWHELP "${PROJECT_BINARY_DIR}/dbghelpdr.dll")
  add_custom_command(
    OUTPUT "${VERYSLEEPY_STAGED_MGWHELP}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:mgwhelp>" "${VERYSLEEPY_STAGED_MGWHELP}"
    DEPENDS "$<TARGET_FILE:mgwhelp>"
    COMMENT "Staging mgwhelp as dbghelpdr.dll"
    VERBATIM
  )
  add_custom_target(stage_mgwhelp ALL DEPENDS "${VERYSLEEPY_STAGED_MGWHELP}")
  add_dependencies(sleepy sleepy_com stage_mgwhelp)
else()
  message(FATAL_ERROR "The drmingw submodule did not define the mgwhelp target.")
endif()

get_filename_component(VERYSLEEPY_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(VERYSLEEPY_PACKAGE_FILE_NAME "verysleepy-${VERYSLEEPY_VERSION}-x64")

install(
  TARGETS sleepy
  RUNTIME_DEPENDENCY_SET sleepy_runtime_dependencies
  RUNTIME DESTINATION .
)

install(TARGETS sleepy_com RUNTIME DESTINATION .)

install(
  RUNTIME_DEPENDENCY_SET sleepy_runtime_dependencies
  DESTINATION .
  PRE_EXCLUDE_REGEXES
    "api-ms-.*"
    "ext-ms-.*"
  POST_EXCLUDE_REGEXES
    ".*[/\\]system32[/\\].*\\.dll"
    ".*[/\\]SysWOW64[/\\].*\\.dll"
    ".*[/\\]Windows[/\\]WinSxS[/\\].*\\.dll"
    ".*[/\\]msys64[/\\]usr[/\\]bin[/\\].*\\.dll"
  DIRECTORIES
    "${PROJECT_BINARY_DIR}"
    "${VERYSLEEPY_MINGW_BIN_DIR}"
)

# dbghelpdr.dll parses DWARF and PE exports itself and forwards everything else,
# CodeView/PDB included, to a real dbghelp. It imports that under the private name
# dbghelpms.dll (see MGWHELP_DBGHELP_DLL in the drmingw submodule) rather than
# dbghelp.dll, because a dbghelp.dll here would shadow the OS one for the whole
# process and dbgeng.dll, which sleepy.exe imports, fails to load against an older
# copy. dbghelp finds these beside itself: symsrv.dll for the symbol server,
# srcsrv.dll for the source server, and symsrv.yes to accept the server EULA up
# front rather than prompting.
install(
  FILES
    "${PROJECT_SOURCE_DIR}/dbghelp_x64/dbghelpms.dll"
    "${PROJECT_SOURCE_DIR}/dbghelp_x64/symsrv.dll"
    "${PROJECT_SOURCE_DIR}/dbghelp_x64/srcsrv.dll"
    "${PROJECT_SOURCE_DIR}/dbghelp_x64/symsrv.yes"
  DESTINATION .
)

install(
  FILES
    "${PROJECT_SOURCE_DIR}/keywords.txt"
    "${PROJECT_SOURCE_DIR}/license.txt"
    "${PROJECT_SOURCE_DIR}/osfunctions.txt"
    "${PROJECT_SOURCE_DIR}/osmodules.txt"
  DESTINATION .
)

install(
  FILES "$<TARGET_FILE:mgwhelp>"
  DESTINATION .
  RENAME dbghelpdr.dll
)

install(FILES "${PROJECT_BINARY_DIR}/dbghelpw.dll" DESTINATION . OPTIONAL)
install(FILES "${PROJECT_BINARY_DIR}/dbghelpw_wow64.dll" DESTINATION . OPTIONAL)

set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_VENDOR "${VERYSLEEPY_VENDOR}")
set(CPACK_PACKAGE_NAME "verysleepy")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERYSLEEPY_FILE_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERYSLEEPY_FILE_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERYSLEEPY_FILE_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${VERYSLEEPY_APPNAME}")
set(CPACK_PACKAGE_EXECUTABLES "sleepy" "${VERYSLEEPY_APPNAME}")
set(CPACK_CREATE_DESKTOP_LINKS "sleepy")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_FILE_NAME "${VERYSLEEPY_PACKAGE_FILE_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${VERYSLEEPY_APPNAME} profiler")
set(CPACK_PACKAGE_HOMEPAGE_URL "${VERYSLEEPY_APPURL}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/license.txt")
# Both of these name the product, not the build: CPACK_NSIS_PACKAGE_NAME becomes the
# installer title and, through it, the default start menu folder, and DISPLAY_NAME is
# what Add/Remove Programs lists. The build goes in DisplayVersion beside it, which is
# where Windows expects to find it and otherwise only ever read 0.0.0.
set(CPACK_NSIS_DISPLAY_NAME "${VERYSLEEPY_APPNAME}")
set(CPACK_NSIS_PACKAGE_NAME "${VERYSLEEPY_APPNAME}")
set(CPACK_PACKAGE_VERSION "${VERYSLEEPY_VERSION}")
set(CPACK_NSIS_CONTACT "${VERYSLEEPY_VENDOR}")
set(CPACK_NSIS_URL_INFO_ABOUT "${VERYSLEEPY_APPURL}")
set(CPACK_NSIS_HELP_LINK "${VERYSLEEPY_APPURL}")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_INSTALLED_ICON_NAME "sleepy.exe")
set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/src/res/sleepy.ico")
set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/src/res/sleepy.ico")
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "sleepy.exe")
string(CONCAT CPACK_NSIS_DEFINES
  "VIProductVersion \\\"${VERYSLEEPY_FILE_VERSION_MAJOR}.${VERYSLEEPY_FILE_VERSION_MINOR}.${VERYSLEEPY_FILE_VERSION_PATCH}.${GIT_VERSION_DIRTY}\\\"\n"
  "VIAddVersionKey \\\"ProductName\\\" \\\"${VERYSLEEPY_APPNAME}\\\"\n"
  "VIAddVersionKey \\\"ProductVersion\\\" \\\"${VERYSLEEPY_VERSION}\\\"\n"
  "VIAddVersionKey \\\"CompanyName\\\" \\\"${VERYSLEEPY_VENDOR}\\\"\n"
  "VIAddVersionKey \\\"FileDescription\\\" \\\"${VERYSLEEPY_APPNAME} installer\\\"\n"
  "VIAddVersionKey \\\"FileVersion\\\" \\\"${VERYSLEEPY_VERSION}\\\"\n"
  "VIAddVersionKey \\\"OriginalFilename\\\" \\\"${VERYSLEEPY_PACKAGE_FILE_NAME}.exe\\\""
)

include(CPack)
