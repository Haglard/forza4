cmake_minimum_required(VERSION 3.16)
project(forza4 C OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
include_directories(include)

# ================================================================
# core — generic utilities (shared with chess-engine / dama)
# ================================================================
add_library(core STATIC
    src/core/log.c src/core/vec.c src/core/hashmap.c src/core/time.c
    src/core/rng.c src/core/bitops.c src/core/ringbuf.c src/core/pool.c
    src/game/api.c src/game/nim.c
)
target_include_directories(core PUBLIC include)

# ================================================================
# engine — generic negamax / alpha-beta / TT
# ================================================================
add_library(engine STATIC
    src/engine/search.c src/engine/tt.c
)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE core)

# ================================================================
# connect4 — board, movegen, eval, zobrist
# ================================================================
add_library(connect4 STATIC
    src/connect4/board.c src/connect4/movegen.c
    src/connect4/eval.c  src/connect4/zobrist.c
)
target_include_directories(connect4 PUBLIC include)
target_link_libraries(connect4 PRIVATE core)

# ================================================================
# game_connect4 — GameAPI adapter
# ================================================================
add_library(game_connect4 STATIC
    src/game/connect4_adapter.c
)
target_include_directories(game_connect4 PUBLIC include)
target_link_libraries(game_connect4 PRIVATE connect4 core)

# ================================================================
# CLI tools
# ================================================================
add_executable(search_cli tools/search_cli.c)
target_link_libraries(search_cli PRIVATE game_connect4 connect4 engine core)

add_executable(selfplay_cli tools/selfplay_cli.c)
target_link_libraries(selfplay_cli PRIVATE game_connect4 connect4 engine core)

# ================================================================
# macOS GUI client
# ================================================================
if(APPLE)
    enable_language(OBJC)
    add_executable(connect4_client MACOSX_BUNDLE tools/connect4_client_cocoa.m)
    target_include_directories(connect4_client PRIVATE include)
    target_compile_options(connect4_client PRIVATE -fobjc-arc)
    target_link_libraries(connect4_client PRIVATE
        game_connect4 connect4 engine core
        "-framework AppKit" "-framework Foundation")
    set_target_properties(connect4_client PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME          "Vibe Forza 4"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.haglard.vibeforza4"
        MACOSX_BUNDLE_BUNDLE_VERSION       "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_COPYRIGHT            "© 2026 Haglard"
        OUTPUT_NAME                        "Vibe Forza 4"
    )
    add_custom_target(dmg
        DEPENDS connect4_client
        COMMAND ${CMAKE_COMMAND} -E remove -f
                "${CMAKE_BINARY_DIR}/VibeForza4-1.0.dmg"
        COMMAND hdiutil create
                -volname   "Vibe Forza 4 1.0"
                -srcfolder "${CMAKE_BINARY_DIR}/Vibe Forza 4.app"
                -ov -format UDZO
                "${CMAKE_BINARY_DIR}/VibeForza4-1.0.dmg"
        COMMENT "Creating VibeForza4-1.0.dmg …"
        VERBATIM
    )
endif()

enable_testing()
