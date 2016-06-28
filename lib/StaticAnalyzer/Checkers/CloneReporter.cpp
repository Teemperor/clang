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
class CloneReporter : public Checker<check::EndOfTranslationUnit> {
public:
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                 AnalysisManager &Mgr, BugReporter &BR) const;
};
} // end anonymous namespace

void CloneReporter::checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                                  AnalysisManager &Mgr,
                                                  BugReporter &BR) const {
  ASTStructure Structure(TU->getASTContext());

  std::vector<ASTStructure::CloneGroup> CloneGroups = Structure.FindClones();

  DiagnosticsEngine& DiagEngine = Mgr.getDiagnostic();

  unsigned WarnID =
      DiagEngine.getCustomDiagID(DiagnosticsEngine::Warning,
                                 "Detected code clone.");

  unsigned NoteID =
      DiagEngine.getCustomDiagID(DiagnosticsEngine::Note,
                                 "Related code clone is here.");

  for (ASTStructure::CloneGroup& Group : CloneGroups) {
    DiagEngine.Report(Group.front().getLocStart(), WarnID);
    for (unsigned J = 1; J < Group.size(); ++J) {
      DiagEngine.Report(Group[J].getLocStart(), NoteID);
    }
  }
}

//===----------------------------------------------------------------------===//
// Register CloneReporter
//===----------------------------------------------------------------------===//

void ento::registerCloneReporter(CheckerManager &Mgr) {
  Mgr.registerChecker<CloneReporter>();
}
