# Unit tests (enable with -DBUILD_UNIT_TESTS=ON)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_executable(test_daptransport
    test_daptransport.cpp
    ${CMAKE_SOURCE_DIR}/RedPandaIDE/src/debugger/daptransport.cpp
)
target_include_directories(test_daptransport PRIVATE ${CMAKE_SOURCE_DIR}/RedPandaIDE/src)
target_link_libraries(test_daptransport PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
set_target_properties(test_daptransport PROPERTIES AUTOMOC ON)
add_test(NAME daptransport COMMAND test_daptransport)
