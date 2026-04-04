find_package(Catch2 REQUIRED CONFIG)
find_package(Boost REQUIRED)

file(GLOB test_src test_*.cpp)

add_executable(all_test main.cpp ${test_src})

target_link_libraries(all_test PRIVATE 
    Catch2::Catch2 
    Catch2::Catch2WithMain 
    uuidv7pp::uuidv7pp
    Boost::boost
)

target_compile_features(all_test PRIVATE ${STD_CPP})

add_test(NAME all_test COMMAND all_test -r junit)
