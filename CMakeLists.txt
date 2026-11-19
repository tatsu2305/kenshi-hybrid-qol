cmake_minimum_required(VERSION 3.20)
project(KenshiHybridQoL VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Ensure 64-bit build for Kenshi 1.0.65 x64
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Kenshi 1.0.65 requires an x64 (64-bit) build target. Configure with -A x64.")
endif()

if(MSVC)
    add_compile_options(/W4 /MP /EHsc /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Fetch MinHook automatically if not provided locally
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_MakeAvailable(minhook)

if(NOT TARGET minhook)
    add_library(minhook STATIC
        ${minhook_SOURCE_DIR}/src/buffer.c
        ${minhook_SOURCE_DIR}/src/hook.c
        ${minhook_SOURCE_DIR}/src/trampoline.c
        ${minhook_SOURCE_DIR}/src/hde/hde64.c
    )
    target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)
endif()

set(SOURCES
    src/dllmain.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/PatternScanner.cpp
    src/core/HookManager.cpp
    src/sdk/GameApi.cpp
    src/features/BulkHauling.cpp
    src/features/AntiGateClipping.cpp
    src/features/TacticalEscort.cpp
    src/features/BaseKnockout.cpp
    src/features/SmartLootSmelt.cpp
    src/features/TurretDiscipline.cpp
)

set(HEADERS
    include/sdk/KenshiSDK.h
    include/sdk/Signatures.h
    include/sdk/SignaturesPending.h
    include/sdk/GameApi.h
    include/core/Config.h
    include/core/Logger.h
    include/core/PatternScanner.h
    include/core/HookManager.h
    include/features/Features.h
)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${minhook_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    minhook
    psapi
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "KenshiHybridQoL"
    PREFIX ""
    SUFFIX ".dll"
)

# Copy config.ini to output directory post-build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/KenshiHybridQoL.ini"
)



