set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_unittest(SerializationTests
  GenerationCounterTest.cpp
  )

target_link_libraries(SerializationTests
  PRIVATE
  clangAST
  clangSerialization
  clangBasic
  clangFrontend
  clangTooling
  )
