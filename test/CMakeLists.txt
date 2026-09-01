find_package(Catch2 REQUIRED CONFIG)

find_package(Boost REQUIRED CONFIG COMPONENTS uuid)

# Boost::uuid lists atomic in its interface link libraries, which causes
# -latomic to be linked unnecessarily. The UUID header-only library does
# not require libatomic on modern GCC, and the resulting -latomic can
# fail on Fedora 43 where GCC's libatomic linker script references a
# non-existent library version path.
if(TARGET Boost::uuid)
  get_target_property(_uuid_link_libs Boost::uuid INTERFACE_LINK_LIBRARIES)
  if(_uuid_link_libs)
    list(REMOVE_ITEM _uuid_link_libs "atomic")
    set_target_properties(Boost::uuid PROPERTIES INTERFACE_LINK_LIBRARIES "${_uuid_link_libs}")
  endif()
  unset(_uuid_link_libs)
endif()

file(GLOB test_src test_*.cpp)

add_executable(all_test ${test_src})

target_link_libraries(all_test PRIVATE
    Catch2::Catch2WithMain
    uuid7pp::uuid7pp
    Boost::uuid
)

target_compile_features(all_test PRIVATE cxx_std_23)

add_test(NAME all_test COMMAND all_test -r junit --order=decl)

if(ENABLE_FREESTANDING)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # freestanding 検証: Catch2 を使わず libstdc++ リンクなしでビルド・実行する。
        # OS 依存 API (generate / generate_batch / generate_at(time_point) /
        # extract_timestamp(time_point)) の無効化と、seed() + generate_at(uint64_t)
        # のみの構成で動的確保なしに完結することを確認する。
        add_executable(freestanding_check freestanding_check.cpp)
        target_link_libraries(freestanding_check PRIVATE uuid7pp::uuid7pp)
        target_compile_definitions(freestanding_check PRIVATE UUID7PP_FREESTANDING)
        target_compile_options(freestanding_check PRIVATE -ffreestanding -fno-exceptions -fno-rtti)
        target_link_options(freestanding_check PRIVATE -nostdlib++)
        add_test(NAME freestanding_check COMMAND freestanding_check)
    else()
        message(WARNING "ENABLE_FREESTANDING check test requires GNU/Clang")
    endif()
endif()
