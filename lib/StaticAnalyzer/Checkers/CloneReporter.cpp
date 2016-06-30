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

#include <regex>

using namespace clang;
using namespace ento;

namespace {
class CloneReporter : public Checker<check::EndOfTranslationUnit> {

  // A regex that at least one function name used inside a clone
  // has to match to be reported.
  // TODO: Bind this option to an user-accessible option-flag.
  std::regex FunctionFilter = std::regex("[\\s\\S]*");

public:
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                 AnalysisManager &Mgr, BugReporter &BR) const;

  bool ShouldReport(StmtSequence& Stmt) const {
    StmtFeature Features(Stmt);
    auto FeatureVector = Features.GetFeatureVector(StmtFeature::FunctionName);
    for (unsigned I = 0; I < FeatureVector.GetNumberOfNames(); ++I) {
      if (std::regex_match(FeatureVector.GetName(I), FunctionFilter)) {
        return true;
      }
    }
    return false;
  }

  bool ShouldReportGroup(ASTStructure::CloneGroup& Group) const {
    unsigned Matches = 0;
    for (StmtSequence& StmtSeq : Group) {
      if (ShouldReport(StmtSeq)) {
        Matches++;
        if (Matches >= 2) {
          return true;
        }
      }
    }
    return false;
  }
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
    if (ShouldReportGroup(Group)) {
      DiagEngine.Report(Group.front().getLocStart(), WarnID);
      for (unsigned J = 1; J < Group.size(); ++J) {
        DiagEngine.Report(Group[J].getLocStart(), NoteID);
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// Register CloneReporter
//===----------------------------------------------------------------------===//

void ento::registerCloneReporter(CheckerManager &Mgr) {
  Mgr.registerChecker<CloneReporter>();
}
