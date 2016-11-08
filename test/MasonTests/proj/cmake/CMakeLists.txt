cmake_minimum_required( VERSION 3.0 FATAL_ERROR )
project( MasonTests )

set( APP_TARGET "MasonTests" )
set( REPO_PATH "${PROJECT_SOURCE_DIR}/../.." )
set( CMAKE_VERBOSE_MAKEFILE ON )

message( STATUS "REPO_PATH: ${REPO_PATH}" )
message( STATUS "[MasonTests] DART_PATH: ${DART_PATH}" )
message( STATUS "[MasonTests] CINDER_PATH_FULL: ${CINDER_PATH_FULL}" )

include( "${CINDER_PATH_FULL}/proj/cmake/modules/cinderMakeApp.cmake" )

set( APP_SOURCES
	src/MasonTestsApp.cpp
	src/BlendingTest.cpp
	src/EmptyTest.cpp
	src/HorizonTest.cpp
	src/MasonTestsApp.cpp
	src/MiscTest.cpp
	src/MotionTest.cpp
	src/motionjson/MotionJson.cpp
	src/ParticleGeometryTest.cpp
	src/WispSimTest.cpp
)

ci_make_app(
	APP_NAME ${APP_TARGET}
	SOURCES ${APP_SOURCES}
	CINDER_PATH ${CINDER_PATH_FULL}
)

target_include_directories( ${APP_TARGET} PUBLIC
	${PROJECT_SOURCE_DIR}/src
	${REPO_PATH}/src
)

#add_dependencies( ${APP_TARGET} cinder )

target_link_libraries( ${APP_TARGET}
	mason
	cinder-view
	cinder-dart
	${DART_PATH}/dart-runtime/lib/macosx/Release/libdart_runtime.a # TODO: move to cinder-dart cmake as a package
)
