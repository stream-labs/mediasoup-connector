# Project configuration.
set(WEBRTC_INCLUDE_PATH "" CACHE STRING "webrtc include path")
set(WEBRTC_LIB_PATH "" CACHE STRING "webrtc .lib path")
set(MEDIASOUP_INCLUDE_PATH "" CACHE STRING "libmediasoup include path")
set(MEDIASOUP_LIB_PATH "" CACHE STRING "libmediasoup .lib path")
set(MEDIASOUP_SDP_LIB_PATH "" CACHE STRING "sdptransform .lib path")
set(MEDIASOUP_SDP_INCLUDE_PATH "" CACHE STRING "sdptransform include path")

find_package(OpenSSL)

if(NOT WEBRTC_INCLUDE_PATH)
	message(FATAL_ERROR "mediasoup-connector: WEBRTC_INCLUDE_PATH not provided")
endif()
file(TO_CMAKE_PATH ${WEBRTC_INCLUDE_PATH} WEBRTC_INCLUDE_PATH)

if(NOT WEBRTC_LIB_PATH)
	message(FATAL_ERROR  "mediasoup-connector: WEBRTC_LIB_PATH missing")
endif()
file(TO_CMAKE_PATH ${WEBRTC_LIB_PATH} WEBRTC_LIB_PATH)

if(NOT MEDIASOUP_INCLUDE_PATH)
	message(FATAL_ERROR  "mediasoup-connector: MEDIASOUP_INCLUDE_PATH missing")
endif()
file(TO_CMAKE_PATH ${MEDIASOUP_INCLUDE_PATH} MEDIASOUP_INCLUDE_PATH)

if(NOT MEDIASOUP_LIB_PATH)
	message(FATAL_ERROR  "mediasoup-connector: MEDIASOUP_LIB_PATH missing")
endif()
file(TO_CMAKE_PATH ${MEDIASOUP_LIB_PATH} MEDIASOUP_LIB_PATH)

if(NOT MEDIASOUP_SDP_LIB_PATH)
	message(FATAL_ERROR  "mediasoup-connector: MEDIASOUP_SDP_LIB_PATH missing")
endif()
file(TO_CMAKE_PATH ${MEDIASOUP_SDP_LIB_PATH} MEDIASOUP_SDP_LIB_PATH)

if(NOT OPENSSL_FOUND)
	message(FATAL_ERROR "mediasoup-connector: OPENSSL_FOUND failed")
endif()

if (APPLE)
	foreach(var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
		CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
		if(${var} MATCHES "/MD")
			string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
		endif()
	endforeach()
endif()

set(webrtc_COMMON_COMPILE_DEFS NOMINMAX)

if(MSVC)
	list(APPEND webrtc_COMMON_COMPILE_DEFS WEBRTC_WIN WIN32_LEAN_AND_MEAN)
endif()

if(APPLE)
	list(APPEND webrtc_COMMON_COMPILE_DEFS WEBRTC_POSIX)
endif()

# ----------------
# -- webrtc LIB --
# ----------------

set(webrtc-test_SOURCES
	${WEBRTC_INCLUDE_PATH}/api/test/create_frame_generator.cc
	${WEBRTC_INCLUDE_PATH}/media/base/fake_frame_source.cc
	${WEBRTC_INCLUDE_PATH}/pc/test/fake_audio_capture_module.cc
	${WEBRTC_INCLUDE_PATH}/rtc_base/task_queue_for_test.cc
	${WEBRTC_INCLUDE_PATH}/test/frame_generator.cc
	${WEBRTC_INCLUDE_PATH}/test/frame_generator_capturer.cc
	${WEBRTC_INCLUDE_PATH}/test/frame_utils.cc
	${WEBRTC_INCLUDE_PATH}/test/test_video_capturer.cc
	${WEBRTC_INCLUDE_PATH}/test/testsupport/file_utils.cc
	${WEBRTC_INCLUDE_PATH}/test/testsupport/file_utils_override.cc
	${WEBRTC_INCLUDE_PATH}/test/testsupport/ivf_video_frame_generator.cc
	${WEBRTC_INCLUDE_PATH}/test/vcm_capturer.cc)

include_directories(${WEBRTC_INCLUDE_PATH})
include_directories(${WEBRTC_INCLUDE_PATH}/third_party/abseil-cpp)
include_directories(${WEBRTC_INCLUDE_PATH}/third_party/libyuv/include)

add_library(webrtc-testlibs STATIC
	${webrtc-test_SOURCES})

# webrtc's warnings, we don't need to see them)
if(MSVC)
	target_compile_options(webrtc-testlibs PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>" /wd4100 /wd4244 /wd4267 /wd4099)
endif()

target_link_libraries(webrtc-testlibs
	${WEBRTC_LIB_PATH})

set_property(TARGET webrtc-testlibs PROPERTY CXX_STANDARD 14)

target_compile_definitions(webrtc-testlibs PUBLIC ${webrtc_COMMON_COMPILE_DEFS})

# ----------------------
# -- mediasoup PLUGIN --
# ----------------------

project(mediasoup-connector)
set(MODULE_DESCRIPTION "Streamlabs Join")

if(MSVC)
	set(mediasoup-connector_PLATFORM_DEPS
		w32-pthreads)
endif()

# WEBRTC_INCLUDE_PATH must use forward slashes
set(mediasoup-connector_SOURCES
	mediasoup-connector.cpp
	ConnectorFrontApi.h
	ConnectorFrontApi.cpp
	ConnectorFrontApiHelper.cpp
	MediaSoupTransceiver.cpp
	MediaSoupTransceiver.h
	MediaSoupInterface.cpp
	MediaSoupInterface.h
	MyProducerAudioDeviceModule.h
	MediaSoupMailbox.h
	MediaSoupMailbox.cpp
	MyFrameGeneratorInterface.cpp
	MyFrameGeneratorInterface.h
	MyLogSink.cpp
	MyLogSink.h)

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${MEDIASOUP_INCLUDE_PATH})
include_directories(${MEDIASOUP_SDP_INCLUDE_PATH})

include_directories(${WEBRTC_INCLUDE_PATH})
include_directories(${WEBRTC_INCLUDE_PATH}/third_party/abseil-cpp)
include_directories(${WEBRTC_INCLUDE_PATH}/third_party/libyuv/include)

add_library(mediasoup-connector MODULE
	${mediasoup-connector_SOURCES})

if(MSVC)
    target_compile_options(mediasoup-connector PRIVATE /W3 /wd4100 /wd4244 /wd4099 "$<IF:$<CONFIG:Debug>,/MTd,/MT>" /WX-)
    target_link_options(mediasoup-connector PRIVATE /IGNORE:4099)
endif()

target_link_libraries(mediasoup-connector
	libobs
	webrtc-testlibs
	${WEBRTC_LIB_PATH}
	${MEDIASOUP_LIB_PATH}
	${MEDIASOUP_SDP_LIB_PATH}
	${mediasoup-connector_PLATFORM_DEPS})

set_target_properties(mediasoup-connector PROPERTIES FOLDER "plugins")
set_property(TARGET mediasoup-connector PROPERTY CXX_STANDARD 14)

target_compile_definitions(mediasoup-connector PUBLIC ${webrtc_COMMON_COMPILE_DEFS})

if(APPLE)
	include_directories(mediasoup-connector mac)
	
	find_library(APPLE_CoreServices CoreServices)
	find_library(APPLE_CoreFoundation CoreFoundation)
	find_library(APPLE_AudioUnit AudioUnit)
	find_library(APPLE_AudioToolbox AudioToolbox)
	find_library(APPLE_CoreAudio CoreAudio)
	find_library(APPLE_Foundation Foundation)
	find_library(APPLE_ApplicationServices ApplicationServices)
	
	if (NOT APPLE_CoreServices)
		message(FATAL_ERROR "APPLE_CoreServices not found")
	endif()

	if (NOT APPLE_CoreFoundation)
		message(FATAL_ERROR "APPLE_CoreFoundation not found")
	endif()

	if (NOT APPLE_AudioUnit)
		message(FATAL_ERROR "APPLE_AudioUnit not found")
	endif()

	if (NOT APPLE_AudioUnit)
		message(FATAL_ERROR "APPLE_AudioUnit not found")
	endif()

	if (NOT APPLE_AudioToolbox)
		message(FATAL_ERROR "APPLE_AudioToolbox not found")
	endif()

	if (NOT APPLE_CoreAudio)
		message(FATAL_ERROR "APPLE_CoreAudio not found")
	endif()

	if (NOT APPLE_Foundation)
		message(FATAL_ERROR "APPLE_Foundation not found")
	endif()

	if (NOT APPLE_ApplicationServices)
		message(FATAL_ERROR "APPLE_ApplicationServices not found")
	endif()

	target_link_libraries(mediasoup-connector ${APPLE_CoreServices})
	target_link_libraries(mediasoup-connector ${APPLE_CoreFoundation})
	target_link_libraries(mediasoup-connector ${APPLE_AudioUnit})
	target_link_libraries(mediasoup-connector ${APPLE_AudioToolbox})
	target_link_libraries(mediasoup-connector ${APPLE_CoreAudio})
	target_link_libraries(mediasoup-connector ${APPLE_Foundation})
	target_link_libraries(mediasoup-connector ${APPLE_ApplicationServices})
	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++ -Wno-deprecated-declarations")
endif()

#set_target_properties(webrtc-testlibs PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG TRUE)
#set_target_properties(mediasoup-connector PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG TRUE)

setup_plugin_target(mediasoup-connector)