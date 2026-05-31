add_executable(tetris_test
    test_main.cpp
    test_rotation.cpp
    test_well.cpp
    test_clear.cpp
    test_game.cpp
    test_ai.cpp
)
target_link_libraries(tetris_test PRIVATE tetris_core Catch2::Catch2WithMain)
target_include_directories(tetris_test PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME tetris_test COMMAND tetris_test)
