//===--- CloneDetection.cpp - Clone detection checkers ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This defines Clone Chechers, they warn about equal or similar pieces
/// of code, which is considered to be a bad programming practice and may lead
/// to bugs and errors.
///
//===----------------------------------------------------------------------===//

#include "ClangSACheckers.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/LangOptions.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// BasicCloneDetectionChecker
//===----------------------------------------------------------------------===//

namespace {
class BasicCloneChecker : public Checker<check::EndOfTranslationUnit> {
public:
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                 AnalysisManager &Mgr, BugReporter &BR) const;
};
} // end anonymous namespace

void BasicCloneChecker::checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                                  AnalysisManager &Mgr,
                                                  BugReporter &BR) const {
  ASTStructure Structure(TU->getASTContext());

  std::vector<StmtFeature::CompareResult> Clones = Structure.findCloneErrors();

  DiagnosticsEngine& DiagEngine = Mgr.getDiagnostic();

  for (StmtFeature::CompareResult Clone : Clones) {
      unsigned WarnID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Warning, "%0");
      unsigned NoteID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Note, "%0");

      SourceLocation WarnLoc = Clone.result.FeatureThis.getLocation();
      SourceLocation NoteLoc = Clone.result.FeatureOther.getLocation();

      DiagEngine.Report(WarnLoc, WarnID) << "Possibly wrong copy-pasted code";

      DiagEngine.Report(NoteLoc, NoteID) << "Copy-paste source was here";

  }
}

//===----------------------------------------------------------------------===//
// Register BasicCloneChecker
//===----------------------------------------------------------------------===//

void ento::registerCopyPasteChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<CopyPasteChecker>();
}
