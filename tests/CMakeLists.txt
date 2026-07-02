# Jednotkové testy (Qt Test). Spuštění: ctest --test-dir build  nebo přímo
# ./build/tests/picture_viewer_tests

find_package(Qt6 REQUIRED COMPONENTS Test Sql Multimedia MultimediaWidgets)

qt_add_executable(picture_viewer_tests
    test_core.cpp
    $<TARGET_OBJECTS:picture_viewer_app>
    $<TARGET_OBJECTS:picture_viewer_workers>
)

target_include_directories(picture_viewer_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(picture_viewer_tests
    PRIVATE
        picture_viewer_core
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Network
        Qt6::Pdf
        Qt6::Sql
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)

add_test(NAME core_tests COMMAND picture_viewer_tests)
