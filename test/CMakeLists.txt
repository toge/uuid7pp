find_package(Catch2 REQUIRED CONFIG)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost REQUIRED CONFIG COMPONENTS uuid)

file(GLOB test_src test_*.cpp)

add_executable(all_test ${test_src})

target_link_libraries(all_test PRIVATE 
    Catch2::Catch2WithMain 
    uuidv7pp::uuidv7pp
    Boost::uuid
)

target_compile_features(all_test PRIVATE ${STD_CPP})

add_test(NAME all_test COMMAND all_test -r junit)
