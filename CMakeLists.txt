cmake_minimum_required(VERSION 3.21)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/buildOutput)

#I am using vcpkg (in manifest mode) to obtain SDL2 and ImGui libraries.
if(EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(VCPKG_TARGET_ARCHITECTURE x64)
	set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed")
else()
    message(FATAL_ERROR 
        "vcpkg could not be found. If you already have vcpkg installed,
		then set the environment variable VCPKG_ROOT to the absolute path
		where vcpkg was installed. For some reason vcpkg does not set this 
		environment variable automatically when it gets installed...
        On windows you might have to make sure that in the VCPKG_ROOT variable
        you have double backslashes (I have noticed some cmake bugs if they are single backslashes)"
    )
endif()

project(Chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set(HEADER_FILES
    src/hpp/Board.hpp
    src/hpp/castleRights.hpp
    src/hpp/ChessEvents.hpp
    src/hpp/ChessMove.hpp
    src/hpp/chessNetworkProtocol.h
    src/hpp/ChessRenderer.hpp
    src/hpp/ConnectionManager.hpp
    src/hpp/errorLogger.hpp
    src/hpp/PieceTypes.hpp
    src/hpp/PopupManager.hpp
    src/hpp/ServerConnection.hpp
    src/hpp/SettingsFileManager.hpp
    src/hpp/SoundManager.hpp
    src/hpp/TextureManager.hpp
    src/hpp/Vector2i.hpp
    src/hpp/Window.hpp
)

set(CPP_FILES
    src/cpp/Board.cpp
    src/cpp/CastleRights.cpp
    src/cpp/ChessRenderer.cpp
    src/cpp/ConnectionManager.cpp
    src/cpp/main.cpp
    src/cpp/PieceTypes.cpp
    src/cpp/PopupManager.cpp
    src/cpp/ServerConnection.cpp
    src/cpp/SettingsFileManager.cpp
    src/cpp/SoundManager.cpp
    src/cpp/TextureManager.cpp
    src/cpp/Window.cpp
)

add_executable(${PROJECT_NAME} ${CPP_FILES} ${HEADER_FILES})

source_group(header_files FILES ${HEADER_FILES})
source_group(cpp_files FILES ${CPP_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC src/hpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG_BUILD>)

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()
#todo... link with linux socket libraries (and modify the minimal winsock code to work with linux too)

#set(CMAKE_FIND_DEBUG_MODE True)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} 
        PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
        SDL2_image::SDL2_image
    )
else(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME} 
        PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
        SDL2_image::SDL2_image-static
    )
endif()

add_custom_target(copy_resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
	"${CMAKE_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
)
add_dependencies(${PROJECT_NAME} copy_resources)