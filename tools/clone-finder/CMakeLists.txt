set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(clang-clone-finder
  CloneFinder.cpp
  )

target_link_libraries(clang-clone-finder
  PRIVATE
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  )

install(TARGETS clang-clone-finder
  RUNTIME DESTINATION bin)
