find_package(GTest REQUIRED)

add_executable(translator_lookup_test
    translator_lookup_test.cpp
    ../src/translator.cpp
    ../src/translator.h
)

target_include_directories(translator_lookup_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(translator_lookup_test
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Sql
        KF6::I18n
        GTest::gtest
)

target_compile_features(translator_lookup_test PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(translator_lookup_test
    DISCOVERY_MODE PRE_TEST
)
