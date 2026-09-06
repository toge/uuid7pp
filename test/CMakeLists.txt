# 例外なしスモークテスト（wasip1 含む全ターゲットでビルド可能）
# -fno-exceptions でもコンパイル・実行できることを検証
add_executable(smoke_no_exceptions smoke_no_exceptions.cpp)
target_link_libraries(smoke_no_exceptions PRIVATE uuid7pp::uuid7pp)
target_compile_features(smoke_no_exceptions PRIVATE cxx_std_23)
target_compile_options(smoke_no_exceptions PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions>)
add_test(NAME smoke_no_exceptions COMMAND smoke_no_exceptions)

if(UUID7PP_WASIP1_TARGET)
  # wasip1 ターゲットでは Catch2 を使わず smoke test のみで動作確認
  return()
endif()

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
