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

  // If true, only display clone groups where at least one clone matches the
  // given line number and file name.
  // Used when a piece of code needs to be refactored and finding similar
  // pieces of codes
  // TODO: Bind this option to an user-accessible option-flag.
  bool FilterByLocation = false;
  unsigned LineNumberFilter = 0;
  // The suffix that the file needs to have to be reported.
  std::string FilenameFilter;

  // A regex that at least one function name used inside a clone
  // has to match to be reported.
  // Intended to be used by library developers which want to find if users
  // of their libraries need to use boilerplate.
  // TODO: Bind this option to an user-accessible option-flag.
  bool FilterByFunction = false;
  std::regex FunctionFilter = std::regex("[\\s\\S]*");

public:
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                 AnalysisManager &Mgr, BugReporter &BR) const;

  bool MatchesFunctionFilter(StmtSequence& Stmt) const {
    StmtFeature Features(Stmt);
    auto FeatureVector = Features.GetFeatureVector(StmtFeature::FunctionName);
    for (unsigned I = 0; I < FeatureVector.GetNumberOfNames(); ++I) {
      if (std::regex_match(FeatureVector.GetName(I), FunctionFilter)) {
        return true;
      }
    }
    return false;
  }

  bool MatchesLocationFilter(StmtSequence& Stmt) const {
    auto& SM = Stmt.GetASTContext().getSourceManager();
    bool StartInvalid, EndInvalid;

    unsigned StartLineNumber = SM.getPresumedLineNumber(Stmt.getLocStart(),
                                                        &StartInvalid);

    unsigned EndLineNumber = SM.getPresumedLineNumber(Stmt.getLocEnd(),
                                                      &EndInvalid);

    if (!StartInvalid && !EndInvalid) {
      if (SM.getFilename(Stmt.getLocStart()).endswith(FilenameFilter) &&
          StartLineNumber <= LineNumberFilter &&
          EndLineNumber >= LineNumberFilter) {
        return true;
      }
    }
    return false;
  }

  ASTStructure::CloneGroup FilterGroup(ASTStructure::CloneGroup& Group) const {
    if (FilterByFunction) {
      ASTStructure::CloneGroup Result;
      for (StmtSequence& StmtSeq : Group) {
        if (MatchesFunctionFilter(StmtSeq)) {
          Result.push_back(StmtSeq);
        }
      }
      return Result;
    } else if (FilterByLocation) {
      for (StmtSequence& StmtSeq : Group) {
        if (MatchesLocationFilter(StmtSeq)) {
          return Group;
        }
      }
      return ASTStructure::CloneGroup();
    }
    return Group;
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

  for (ASTStructure::CloneGroup& UnfilteredGroup : CloneGroups) {
    auto Group = FilterGroup(UnfilteredGroup);
    if (Group.size() > 1) {
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
