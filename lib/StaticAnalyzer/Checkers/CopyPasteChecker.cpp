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
#include "clang/AST/ASTStructure.h"

using namespace clang;
using namespace ento;
namespace {
class CopyPasteChecker: public Checker<check::EndOfTranslationUnit> {
public:
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                 AnalysisManager &Mgr, BugReporter &BR) const;
};
} // end anonymous namespace

#include <iostream>

void CopyPasteChecker::checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                                  AnalysisManager &Mgr,
                                                  BugReporter &BR) const {
  ASTStructure Structure(TU->getASTContext());

  std::vector<ASTStructure::CloneMismatch> Clones = Structure.findCloneErrors();

  DiagnosticsEngine& DiagEngine = Mgr.getDiagnostic();

  for (ASTStructure::CloneMismatch Clone : Clones) {
      unsigned WarnID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Warning,
                                     "Possibly faulty copy-pasted code");
      unsigned NoteID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Note,
                                    "Copy-paste source was here");

      SourceLocation WarnLoc = Clone.MismatchA.getLocation();
      SourceLocation NoteLoc = Clone.MismatchB.getLocation();

      DiagEngine.Report(WarnLoc, WarnID);
      DiagEngine.Report(NoteLoc, NoteID);
  }
}

//===----------------------------------------------------------------------===//
// Register BasicCloneChecker
//===----------------------------------------------------------------------===//

void ento::registerCopyPasteChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<CopyPasteChecker>();
}
