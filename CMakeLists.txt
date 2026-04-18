cmake_minimum_required(VERSION 3.16)
project(SpaceInvaders VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Qt6 ───────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ── Executable ────────────────────────────────────────────────────────────────
qt_add_executable(SpaceInvaders WIN32
    main.cpp
    app.rc
)

target_link_libraries(SpaceInvaders PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ── MSVC flags ────────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(SpaceInvaders PRIVATE /W3 /O2 /utf-8)
endif()

# ── Deploy Qt DLLs αυτόματα δίπλα στο .exe ───────────────────────────────────
if(WIN32)
    add_custom_command(TARGET SpaceInvaders POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
                --no-translations
                "$<TARGET_FILE:SpaceInvaders>"
        COMMENT "Running windeployqt..."
    )
endif()
