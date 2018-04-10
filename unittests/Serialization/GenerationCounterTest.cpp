//===- unittest/Serialization/GenerationCounterTest.cpp -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Tests the generation counter in the ExternalASTSource.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/Sema/MultiplexExternalSemaSource.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

namespace {
// Just allows us to easier increment the generation without actually having
// to modify the AST in some way.
class ASTSourceTester : public clang::ExternalSemaSource {
public:
  void testIncrementGeneration(clang::ASTContext &C) { incrementGeneration(); }
};
} // namespace

namespace clang {

TEST(GenerationCounter, MultipleConsumers) {
  // Three sources which always should see the same generation counter value
  // once they have been attached to the ASTContext.
  ASTSourceTester Source1, Source2, NewSource;

  // Create a real ASTContext.
  std::unique_ptr<ASTUnit> ASTUnit = tooling::buildASTFromCode("int main() {}");
  clang::ASTContext &C = ASTUnit->getASTContext();

  // Attach the first two sources with a multiplexer.
  MultiplexExternalSemaSource *Multiplexer =
      new MultiplexExternalSemaSource(Source1, Source2);
  C.setExternalSource(Multiplexer);

  auto OldGeneration = Source1.getGeneration(C);

  // Pretend each source modifies the AST and increments the generation counter.
  // After each step the generation counter needs to be identical for each
  // source (but different than the previous counter value).
  Source1.testIncrementGeneration();
  ASSERT_EQ(Source1.getGeneration(), Source2.getGeneration());
  ASSERT_EQ(Source1.getGeneration(), Multiplexer->getGeneration());
  ASSERT_NE(Source1.getGeneration(), OldGeneration);
  OldGeneration = Source1.getGeneration(C);

  Source2.testIncrementGeneration();
  ASSERT_EQ(Source1.getGeneration(), Source2.getGeneration());
  ASSERT_EQ(Source1.getGeneration(), Multiplexer->getGeneration());
  ASSERT_NE(Source1.getGeneration(), OldGeneration);

  // Just add the last source which should also directly inherit the correct
  // generation counter value.
  Multiplexer->addSource(NewSource);
  ASSERT_EQ(NewSource.getGeneration(), Source1.getGeneration());
  ASSERT_EQ(NewSource.getGeneration(), Source2.getGeneration());
  ASSERT_EQ(NewSource.getGeneration(), Multiplexer->getGeneration());
}

} // namespace clang
