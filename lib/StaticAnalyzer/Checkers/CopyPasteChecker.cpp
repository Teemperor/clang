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

void CopyPasteChecker::checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                                  AnalysisManager &Mgr,
                                                  BugReporter &BR) const {
  ASTStructure Structure(TU->getASTContext());

  std::vector<ASTStructure::CloneMismatch> Clones = Structure.findCloneErrors();

  DiagnosticsEngine& DiagEngine = Mgr.getDiagnostic();

  for (ASTStructure::CloneMismatch Clone : Clones) {
      unsigned WarnID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Warning,
                                     "Possibly faulty code clone.");

      unsigned WarnWithSuggestionID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Warning,
                                     "Possibly faulty code clone. Maybe "
                                     "you wanted to use '%0' instead of '%1'?");

      unsigned NoteID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Note,
                                     "Other possibly faulty code clone instance"
                                     " is here.");

      unsigned NoteWithSuggestionID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Note,
                                     "Other possibly faulty code clone instance"
                                     " is here. Maybe you wanted to use '%0'"
                                     " instead of '%1'?");

      SourceLocation LocA = Clone.A.GetFeature().getStartLocation();
      SourceLocation LocB = Clone.B.GetFeature().getStartLocation();

      if (Clone.A.HasSuggestion() && Clone.B.HasSuggestion()) {
          DiagEngine.Report(LocA, WarnWithSuggestionID) <<
                Clone.A.GetFeature().getRange() <<
                Clone.A.GetSuggestion() << Clone.A.GetFeatureName();

          DiagEngine.Report(LocB, NoteWithSuggestionID) <<
                Clone.B.GetFeature().getRange() <<
                Clone.B.GetSuggestion() << Clone.B.GetFeatureName();

      } else if (Clone.A.HasSuggestion() && !Clone.B.HasSuggestion()) {
          DiagEngine.Report(LocA, WarnWithSuggestionID) <<
                Clone.A.GetFeature().getRange() << Clone.A.GetSuggestion()
                << Clone.A.GetFeatureName();

          DiagEngine.Report(LocB, NoteID) << Clone.B.GetFeature().getRange();

      } else if (!Clone.A.HasSuggestion() && Clone.B.HasSuggestion()) {

          DiagEngine.Report(LocB, NoteWithSuggestionID) <<
                Clone.B.GetFeature().getRange() << Clone.B.GetSuggestion()
                << Clone.B.GetSuggestion();

          DiagEngine.Report(LocA, NoteID) << Clone.A.GetFeature().getRange();
      } else {
          DiagEngine.Report(LocA, WarnID) << Clone.A.GetFeature().getRange();
          DiagEngine.Report(LocA, NoteID) << Clone.B.GetFeature().getRange();
      }
  }
}

//===----------------------------------------------------------------------===//
// Register BasicCloneChecker
//===----------------------------------------------------------------------===//

void ento::registerCopyPasteChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<CopyPasteChecker>();
}
