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
                                     "Possibly faulty code clone.");
      unsigned NoteID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Note,
                                     "Other possibly faulty code clone instance"
                                     " is here.");

      unsigned WarnWithSuggestionID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Warning,
                                     "Possibly faulty code clone. Maybe "
                                     "you wanted to use '%0' instead of '%1'?");
      unsigned NoteWithSuggestionID =
          DiagEngine.getCustomDiagID(DiagnosticsEngine::Note,
                                     "Other possibly faulty code clone instance"
                                     " is here. Maybe you wanted to use '%0'"
                                     " instead of '%1'?");

      SourceLocation LocA = Clone.MismatchA.getStartLocation();
      SourceLocation LocB = Clone.MismatchB.getStartLocation();

      bool HasSuggestionA = Clone.FeaturesA.hasNameForIndex(Clone.MismatchB.getNameIndex());
      bool HasSuggestionB = Clone.FeaturesB.hasNameForIndex(Clone.MismatchA.getNameIndex());


      std::string SuggestedFeatureA, SuggestedFeatureB;
      if (HasSuggestionA)
        SuggestedFeatureA = Clone.FeaturesA.getName(Clone.MismatchB.getNameIndex());

      if (HasSuggestionB)
        SuggestedFeatureB = Clone.FeaturesB.getName(Clone.MismatchA.getNameIndex());

      if (HasSuggestionA && HasSuggestionB) {
          DiagEngine.Report(LocA, WarnWithSuggestionID) <<
                Clone.MismatchA.getRange() <<
                SuggestedFeatureA << Clone.MismatchA.getName();
          DiagEngine.Report(LocB, NoteWithSuggestionID) <<
                Clone.MismatchB.getRange() <<
                SuggestedFeatureB << Clone.MismatchB.getName();
      } else if (HasSuggestionA && !HasSuggestionB) {
          DiagEngine.Report(LocA, WarnWithSuggestionID) <<
                Clone.MismatchA.getRange() << SuggestedFeatureA
                << Clone.MismatchA.getName();
          DiagEngine.Report(LocB, NoteID) << Clone.MismatchB.getRange();
      } else if (!HasSuggestionA && HasSuggestionB) {
          DiagEngine.Report(LocB, NoteWithSuggestionID) <<
                Clone.MismatchB.getRange() << SuggestedFeatureB
                << Clone.MismatchB.getName();
          DiagEngine.Report(LocA, NoteID) << Clone.MismatchA.getRange();
      } else {
          DiagEngine.Report(LocA, WarnID) << Clone.MismatchA.getRange();
          DiagEngine.Report(LocA, NoteID) << Clone.MismatchB.getRange();
      }
  }
}

//===----------------------------------------------------------------------===//
// Register BasicCloneChecker
//===----------------------------------------------------------------------===//

void ento::registerCopyPasteChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<CopyPasteChecker>();
}
