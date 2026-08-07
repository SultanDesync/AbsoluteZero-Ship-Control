cmake_minimum_required(VERSION 3.30)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

set(X_VCPKG_APPLOCAL_DEPS_INSTALL OFF)

project(
  AbsoluteZero
  VERSION 0.2.0
  DESCRIPTION "Mouse Alignment Assist for Starfield"
  LANGUAGES CXX
)

set(PROJECT_FRIENDLY_NAME "AbsoluteZero")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY
)

find_package(CommonLibSF CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIRS SimpleIni.h)

set(shared_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/Plugin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shared/include/Plugin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/RuntimePaths.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shared/include/RuntimePaths.h
)

set(absolute_zero_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/MouseAlignHook.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/MouseAlignHook.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/AlignmentAssist.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/AlignmentAssist.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ConfigPolicy.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/SuiteApi.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/SuiteState.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/SuiteState.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/WorkbenchHost.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/WorkbenchHost.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/AbsoluteWorkbenchAPI.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/AbsoluteZeroAPI.h
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  ${shared_sources}
)

add_library(AbsoluteZero SHARED ${absolute_zero_sources})

target_compile_definitions(AbsoluteZero PRIVATE __CMAKE_COMMONLIBSF_PLUGIN=1 ABSOLUTE_ZERO_EXPORTS)
target_include_directories(AbsoluteZero PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shared/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SIMPLEINI_INCLUDE_DIRS}
)
target_compile_definitions(AbsoluteZero PRIVATE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  WINVER=0x0A00
  _WIN32_WINNT=0x0A00
  SPDLOG_FMT_EXTERNAL
  SPDLOG_NO_EXCEPTIONS
  FMT_ENFORCE_COMPILE_STRING=0
  SPDLOG_COMPILED_LIB
)
target_precompile_headers(AbsoluteZero PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shared/include/PCH.h
  ${SIMPLEINI_INCLUDE_DIRS}/SimpleIni.h
)
target_compile_options(AbsoluteZero PRIVATE
  /EHsc /cgthreads8 /diagnostics:caret /MP /W4 /Zc:__cplusplus /Zc:enumTypes /Zc:inline /Zc:templateScope
  /utf-8 /wd5050 /wd4005
)

if(CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
  target_compile_options(AbsoluteZero PRIVATE /fp:fast /Ob3 /GL /GR- /Gw /Qpar)
  target_link_options(AbsoluteZero PRIVATE /CGTHREADS:8 /INCREMENTAL:NO /OPT:REF,ICF=4)
endif()

target_include_directories(AbsoluteZero SYSTEM PRIVATE
  $<TARGET_PROPERTY:CommonLibSF::CommonLibSF,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(AbsoluteZero PRIVATE CommonLibSF::CommonLibSF spdlog::spdlog)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(BUILD_NAME Debug)
else()
  set(BUILD_NAME Release)
endif()

set(PACKAGE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Plugin${BUILD_NAME})
set(PACKAGE_PLUGIN_DIR ${PACKAGE_ROOT}/Data/SFSE/Plugins)

add_custom_command(
  TARGET AbsoluteZero
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGE_PLUGIN_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AbsoluteZero> ${PACKAGE_PLUGIN_DIR}
)

add_custom_command(
  TARGET AbsoluteZero
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/config/AbsoluteZero.workbench.json
    ${PACKAGE_PLUGIN_DIR}
)

include(CTest)
if(BUILD_TESTING)
  add_executable(config_policy_test tests/config_policy_test.cpp)
  target_compile_features(config_policy_test PRIVATE cxx_std_23)
  target_include_directories(config_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME config_policy COMMAND config_policy_test)

  add_executable(suite_api_layout_test tests/suite_api_layout_test.cpp)
  target_compile_features(suite_api_layout_test PRIVATE cxx_std_23)
  target_include_directories(suite_api_layout_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME suite_api_layout COMMAND suite_api_layout_test)
endif()

add_custom_command(
  TARGET AbsoluteZero
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/config/AbsoluteZero.ini
    ${PACKAGE_PLUGIN_DIR}
)

set(RELEASE_ARCHIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/releases")
set(RELEASE_ARCHIVE_NAME "${PROJECT_NAME}-v${PROJECT_VERSION}-${BUILD_NAME}.zip")

add_custom_target(
  package-mo2
  COMMAND ${CMAKE_COMMAND} -E make_directory ${RELEASE_ARCHIVE_DIR}
  COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${RELEASE_ARCHIVE_DIR}/${RELEASE_ARCHIVE_NAME}" --format=zip "Data"
  WORKING_DIRECTORY ${PACKAGE_ROOT}
  DEPENDS AbsoluteZero
  COMMENT "Creating MO2 deployable archive: ${RELEASE_ARCHIVE_NAME}"
)
