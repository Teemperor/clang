//===--- ASTStructure.cpp - Analyses the structure of an AST ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the StructuralHash class.
//
//===----------------------------------------------------------------------===//

#include <clang/AST/ASTStructure.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/ASTContext.h>

#include <iostream>

using namespace clang;

namespace {

class StructuralHashVisitor
    : public RecursiveASTVisitor<StructuralHashVisitor> {

public:
  explicit StructuralHashVisitor(ASTStructure &Hash, ASTContext &Context)
      : SH(Hash), Context(Context) {}

  ~StructuralHashVisitor() {
    // As each hash value is only saved when the calculation of the next
    // Stmt starts, we need to explicitly save the hash value of
    // the last Stmt here.
    SaveCurrentHash();
  }

  bool shouldTraversePostOrder() const { return true; }

  bool VisitStmt(Stmt *S) {
    // At this point we know that all Visit* calls for the previous Stmt have
    // finished, so we now save the calculated hash before starting
    // to calculate the next hash.
    SaveCurrentHash();

    // VisitStmt is the first method to be called for a new
    // Stmt, so we save what Stmt we are currently processing
    // for SaveCurrentHash.
    CurrentStmt = S;

    // Reset options for the current hash code.
    IgnoreClassHash = false;
    Hash = 0;
    Children = 1;
    SkipHash = false;

    // Reset the initial hash code for this Stmt.
    ClassHash = S->getStmtClass();

    if (shouldSkipStmt(S))
      return Skip();

    // Incorporate the hash values of all child Stmts into the current.
    // Hash value.
    for (Stmt *Child : S->children()) {
      if (Child == nullptr) {
        // We use an placeholder value for missing children.
        CalcHash(313);
        ++Children;
      } else {
        CalcHash(Child);
        ASTStructure::HashSearchResult Result = SH.findHash(Child);
        if (Result.Success) {
          Children += Result.Data.Children;
        } else {
          ++Children;
        }
      }
    }

    return true;
  }

// Define all Visit methods for all possible Stmts.
#define STMT(CLASS, PARENT) bool Visit##CLASS(CLASS *S);
#include "clang/AST/StmtNodes.inc"

private:

  bool shouldSkipStmt(Stmt *S) {
    switch (S->getStmtClass()) {
    case Stmt::FloatingLiteralClass:
    case Stmt::CXXBoolLiteralExprClass:
    case Stmt::ObjCBoolLiteralExprClass:
    case Stmt::IntegerLiteralClass:
      return false;
    default:
      return Context.getSourceManager().IsInAnyMacroBody(S->getLocStart()) ||
             Context.getSourceManager().IsInAnyMacroBody(S->getLocEnd());
    }
  }

  // Marks the current Stmt as no to be processed.
  // Always returns \c true that it can be called
  bool Skip() {
    SkipHash = true;
    return true;
  }

  // Merges the given value into the current hash code.
  void CalcHash(unsigned Value) {
    // We follow the same idea as Java's hashCode():
    // Multiply with an prime, then add the new value to the
    // hash code.
    Hash *= 53;
    Hash += Value;
  }

  void CalcHash(const llvm::StringRef &String) {
    for (char c : String) {
      CalcHash(static_cast<unsigned>(c));
    }
  }

  // Merges the hash code of the given Stmt into the
  // current hash code. Stmts that weren't hashed before by this visitor
  // are ignored.
  void CalcHash(Stmt *S) {
    auto I = SH.findHash(S);
    if (I.Success) {
      CalcHash(I.Data.Hash);
    }
  }

  // Saves the current hash code into the persistent storage of this
  // ASTStructure.
  void SaveCurrentHash() {
    if (SkipHash)
      return;

    if (!IgnoreClassHash) {
      Hash += ClassHash;
    }

    if (auto CS = dyn_cast<CompoundStmt>(CurrentStmt)) {
      if (!CS->body_empty()) {
        for (unsigned Length = 1; Length < CS->size() - 1; ++Length) {
          for (unsigned Pos = 0; Pos < CS->size() - Length; ++Pos) {
            unsigned SubHash = 0;
            unsigned Children = 0;

            for (unsigned I = Pos; I < Pos + Length; ++I) {
                Stmt *Child = CS->body_begin()[I];
                ASTStructure::HashSearchResult Result = SH.findHash(Child);
                if (Result.Success) {
                  SubHash += I * 27 * Result.Data.Hash;
                  Children += Result.Data.Children;
                } else {
                  ++Children;
                }
            }

            SH.add(SubHash, Children, CurrentStmt, Pos, Pos + Length);
          }
        }
      }
    }
    SH.add(Hash, Children, CurrentStmt);
  }

  ASTStructure &SH;
  ASTContext &Context;

  // The current statement that is being hashed at the moment
  // or 0 if there is no statement currently hashed.
  Stmt *CurrentStmt = nullptr;

  // All members specify properties of the hash process for the current
  // Stmt. They are resetted after the Stmt is successfully hased.

  // If the current hash should not be saved for later use.
  bool SkipHash = true;

  // The hash value that is calculated right now.
  unsigned Hash;

  // The number of children of the current stmt
  unsigned Children = 0;

  // If true, the current Hash only depends on custom data of the
  // current Stmt and the child values.
  // Use case for this are implicit Stmts that need their child to be
  // processed but shouldn't influence the hash value of the parent.
  bool IgnoreClassHash = false;

  // A hash value that is unique to the current Stmt class.
  // By default, this value is merged with the \c Hash variable
  // for the resulting
  unsigned ClassHash = 0;
};

#define DEF_STMT_VISIT(CLASS, CODE)                                            \
  bool StructuralHashVisitor::Visit##CLASS(CLASS *S) {                         \
    if (SkipHash)                                                              \
      return true;                                                             \
    { CODE; }                                                                  \
    return true;                                                               \
  }

//--- Builtin functionality ------------------------------------------------//
DEF_STMT_VISIT(ArrayTypeTraitExpr, { CalcHash(S->getTrait()); })
DEF_STMT_VISIT(AsTypeExpr, {})
DEF_STMT_VISIT(AtomicExpr, {
  CalcHash(S->isVolatile());
  CalcHash(S->getOp());
})
DEF_STMT_VISIT(ChooseExpr, {})
DEF_STMT_VISIT(ConvertVectorExpr, {})
DEF_STMT_VISIT(CXXNoexceptExpr, {})
DEF_STMT_VISIT(ExpressionTraitExpr, { CalcHash(S->getTrait()); })
DEF_STMT_VISIT(ShuffleVectorExpr, {})
DEF_STMT_VISIT(PredefinedExpr, { CalcHash(S->getIdentType()); })
DEF_STMT_VISIT(TypeTraitExpr, { CalcHash(S->getTrait()); })
DEF_STMT_VISIT(VAArgExpr, {})

//--- MS properties --------------------------------------------------------//
DEF_STMT_VISIT(MSPropertyRefExpr, {})
DEF_STMT_VISIT(MSPropertySubscriptExpr, {})

//--- Calls ----------------------------------------------------------------//
DEF_STMT_VISIT(CXXOperatorCallExpr, { CalcHash(S->getOperator()); })
DEF_STMT_VISIT(CXXMemberCallExpr, {})
DEF_STMT_VISIT(CallExpr, {})

//--- Invalid code ---------------------------------------------------------//
// We don't support hasing invalid code, so we skip all Stmts representing
// invalid code.
DEF_STMT_VISIT(TypoExpr, { return Skip(); })
DEF_STMT_VISIT(UnresolvedLookupExpr, { return Skip(); })
DEF_STMT_VISIT(UnresolvedMemberExpr, { return Skip(); })
DEF_STMT_VISIT(CXXUnresolvedConstructExpr, { return Skip(); })
DEF_STMT_VISIT(OverloadExpr, { return Skip(); })

//--- Exceptions -----------------------------------------------------------//
DEF_STMT_VISIT(CXXThrowExpr, {})
DEF_STMT_VISIT(CXXCatchStmt, {
  if (S->getExceptionDecl())
    CalcHash(829);
})
DEF_STMT_VISIT(CXXTryStmt, {})
DEF_STMT_VISIT(SEHExceptStmt, {})
DEF_STMT_VISIT(SEHFinallyStmt, {})
DEF_STMT_VISIT(SEHLeaveStmt, {})
DEF_STMT_VISIT(SEHTryStmt, {})

//--- Literals -------------------------------------------------------------//
DEF_STMT_VISIT(CharacterLiteral, {
  // We treat all literals as integer literals
  // as the hash is type independent
  ClassHash = Stmt::StmtClass::IntegerLiteralClass;
})
DEF_STMT_VISIT(FloatingLiteral, {
  // We treat all literals as integer literals
  // as the hash is type independent
  ClassHash = Stmt::StmtClass::IntegerLiteralClass;
})
DEF_STMT_VISIT(ImaginaryLiteral, {
  // We treat all literals as integer literals
  // as the hash is type independent
  ClassHash = Stmt::StmtClass::IntegerLiteralClass;
})
DEF_STMT_VISIT(IntegerLiteral, {})

DEF_STMT_VISIT(ObjCBoolLiteralExpr, {})
DEF_STMT_VISIT(CXXBoolLiteralExpr, {})

DEF_STMT_VISIT(StringLiteral, {})
DEF_STMT_VISIT(ObjCStringLiteral, {})

DEF_STMT_VISIT(CompoundLiteralExpr, {})

DEF_STMT_VISIT(GNUNullExpr, {})
DEF_STMT_VISIT(CXXNullPtrLiteralExpr, {})

// TODO implement hashing for this
DEF_STMT_VISIT(UserDefinedLiteral, { return Skip(); })

//--- C++-OOP Stmts --------------------------------------------------------//
DEF_STMT_VISIT(MemberExpr, {})
DEF_STMT_VISIT(CXXNewExpr, {})
DEF_STMT_VISIT(CXXThisExpr, {})
DEF_STMT_VISIT(CXXConstructExpr, {})
DEF_STMT_VISIT(CXXDeleteExpr, {
  CalcHash(S->isArrayFormAsWritten());
  CalcHash(S->isGlobalDelete());
})
DEF_STMT_VISIT(DesignatedInitExpr, {})
DEF_STMT_VISIT(DesignatedInitUpdateExpr, {})
DEF_STMT_VISIT(NoInitExpr, {})
DEF_STMT_VISIT(InitListExpr, {})
DEF_STMT_VISIT(CXXStdInitializerListExpr, {})

DEF_STMT_VISIT(CXXTemporaryObjectExpr, { IgnoreClassHash = true; })
DEF_STMT_VISIT(CXXDefaultArgExpr, {})
DEF_STMT_VISIT(CXXDefaultInitExpr, {})

//--- Casts ----------------------------------------------------------------//
DEF_STMT_VISIT(CXXFunctionalCastExpr, {})

DEF_STMT_VISIT(CXXNamedCastExpr, {})
DEF_STMT_VISIT(CXXConstCastExpr, {})
DEF_STMT_VISIT(CXXDynamicCastExpr, {})
DEF_STMT_VISIT(CXXReinterpretCastExpr, {})
DEF_STMT_VISIT(CXXStaticCastExpr, {})

DEF_STMT_VISIT(ImplicitCastExpr, { IgnoreClassHash = true; })

DEF_STMT_VISIT(CastExpr, {})
DEF_STMT_VISIT(ExplicitCastExpr, {})
DEF_STMT_VISIT(CStyleCastExpr, {})
DEF_STMT_VISIT(ObjCBridgedCastExpr, { CalcHash(S->getBridgeKind()); })

//--- Miscellaneous Exprs --------------------------------------------------//
DEF_STMT_VISIT(Expr, {})
DEF_STMT_VISIT(ParenExpr, {})
DEF_STMT_VISIT(ArraySubscriptExpr, {})
DEF_STMT_VISIT(BinaryOperator, { CalcHash(S->getOpcode()); })
DEF_STMT_VISIT(UnaryOperator, { CalcHash(S->getOpcode()); })

//--- Control flow ---------------------------------------------------------//
DEF_STMT_VISIT(ForStmt, {})
DEF_STMT_VISIT(GotoStmt, {})
DEF_STMT_VISIT(IfStmt, {})
DEF_STMT_VISIT(WhileStmt, {})
DEF_STMT_VISIT(IndirectGotoStmt, {})
DEF_STMT_VISIT(LabelStmt, { CalcHash(S->getDecl()->getName()); })
DEF_STMT_VISIT(MSDependentExistsStmt, { CalcHash(S->isIfExists()); })
DEF_STMT_VISIT(AddrLabelExpr, { CalcHash(S->getLabel()->getName()); })
DEF_STMT_VISIT(BreakStmt, {})
DEF_STMT_VISIT(CompoundStmt, {})
DEF_STMT_VISIT(ContinueStmt, {})
DEF_STMT_VISIT(DoStmt, {})

DEF_STMT_VISIT(SwitchStmt, {})
DEF_STMT_VISIT(SwitchCase, {})
DEF_STMT_VISIT(CaseStmt, {})
DEF_STMT_VISIT(DefaultStmt, {})

DEF_STMT_VISIT(ReturnStmt, {})

DEF_STMT_VISIT(AbstractConditionalOperator, {})
DEF_STMT_VISIT(BinaryConditionalOperator, {})
DEF_STMT_VISIT(ConditionalOperator, {})

//--- Objective-C ----------------------------------------------------------//
DEF_STMT_VISIT(ObjCArrayLiteral, {})
DEF_STMT_VISIT(ObjCBoxedExpr, {})
DEF_STMT_VISIT(ObjCDictionaryLiteral, {})
DEF_STMT_VISIT(ObjCEncodeExpr, {})
DEF_STMT_VISIT(ObjCIndirectCopyRestoreExpr, { CalcHash(S->shouldCopy()); })
DEF_STMT_VISIT(ObjCIsaExpr, {})
DEF_STMT_VISIT(ObjCIvarRefExpr, {})
DEF_STMT_VISIT(ObjCMessageExpr, {})
DEF_STMT_VISIT(ObjCPropertyRefExpr, {
  CalcHash(S->isSuperReceiver());
  CalcHash(S->isImplicitProperty());
})
DEF_STMT_VISIT(ObjCProtocolExpr, {})
DEF_STMT_VISIT(ObjCSelectorExpr, {})
DEF_STMT_VISIT(ObjCSubscriptRefExpr, {})
DEF_STMT_VISIT(ObjCAtCatchStmt, { CalcHash(S->hasEllipsis()); })
DEF_STMT_VISIT(ObjCAtFinallyStmt, {})
DEF_STMT_VISIT(ObjCAtSynchronizedStmt, {})
DEF_STMT_VISIT(ObjCAtThrowStmt, {})
DEF_STMT_VISIT(ObjCAtTryStmt, {})
DEF_STMT_VISIT(ObjCAutoreleasePoolStmt, {})
DEF_STMT_VISIT(ObjCForCollectionStmt, {})

//--- Miscellaneous Stmts --------------------------------------------------//
DEF_STMT_VISIT(CXXFoldExpr, {
  CalcHash(S->isRightFold());
  CalcHash(S->getOperator());
})
DEF_STMT_VISIT(StmtExpr, {})
DEF_STMT_VISIT(ExprWithCleanups, {})
DEF_STMT_VISIT(GenericSelectionExpr, { CalcHash(S->getNumAssocs()); })
DEF_STMT_VISIT(LambdaExpr, {
  for (const LambdaCapture &C : S->captures()) {
    CalcHash(C.isPackExpansion());
    CalcHash(C.getCaptureKind());
  }
  CalcHash(S->isGenericLambda());
  CalcHash(S->isMutable());
  CalcHash(S->getCallOperator()->param_size());
})
DEF_STMT_VISIT(OpaqueValueExpr, { IgnoreClassHash = true; })
DEF_STMT_VISIT(MaterializeTemporaryExpr, { IgnoreClassHash = true; })
DEF_STMT_VISIT(SubstNonTypeTemplateParmExpr, {})
DEF_STMT_VISIT(SubstNonTypeTemplateParmPackExpr, {})
DEF_STMT_VISIT(DeclStmt, {
  auto numDecls = std::distance(S->decl_begin(), S->decl_end());
  CalcHash(537u + static_cast<unsigned>(numDecls));
})

DEF_STMT_VISIT(CompoundAssignOperator, {})
DEF_STMT_VISIT(CXXBindTemporaryExpr, {})
DEF_STMT_VISIT(NullStmt, {})
DEF_STMT_VISIT(CXXScalarValueInitExpr, {})
DEF_STMT_VISIT(ImplicitValueInitExpr, {})
DEF_STMT_VISIT(OffsetOfExpr, {})
DEF_STMT_VISIT(SizeOfPackExpr, {})
DEF_STMT_VISIT(DeclRefExpr, {})
DEF_STMT_VISIT(DependentScopeDeclRefExpr, {})
DEF_STMT_VISIT(CXXPseudoDestructorExpr, {})
DEF_STMT_VISIT(FunctionParmPackExpr, {})
DEF_STMT_VISIT(ParenListExpr, {})
DEF_STMT_VISIT(PackExpansionExpr, {})
DEF_STMT_VISIT(UnaryExprOrTypeTraitExpr, {})
DEF_STMT_VISIT(PseudoObjectExpr, {})
DEF_STMT_VISIT(GCCAsmStmt, {
  CalcHash(S->isVolatile());
  CalcHash(S->isSimple());
  CalcHash(S->getAsmString()->getString());
  CalcHash(S->getNumOutputs());
  for (unsigned I = 0, N = S->getNumOutputs(); I != N; ++I) {
    CalcHash(S->getOutputName(I));
    VisitStringLiteral(S->getOutputConstraintLiteral(I));
  }
  CalcHash(S->getNumInputs());
  for (unsigned I = 0, N = S->getNumInputs(); I != N; ++I) {
    CalcHash(S->getInputName(I));
    CalcHash(S->getInputConstraintLiteral(I));
  }
  CalcHash(S->getNumClobbers());
  for (unsigned I = 0, N = S->getNumClobbers(); I != N; ++I)
    CalcHash(S->getClobberStringLiteral(I));
})

// TODO: implement hashing custom data of these Stmts
DEF_STMT_VISIT(AsmStmt, {})
DEF_STMT_VISIT(MSAsmStmt, {})
DEF_STMT_VISIT(CXXForRangeStmt, {})
DEF_STMT_VISIT(CapturedStmt, {})
DEF_STMT_VISIT(CoreturnStmt, {})
DEF_STMT_VISIT(CoroutineBodyStmt, {})
DEF_STMT_VISIT(CoroutineSuspendExpr, {})
DEF_STMT_VISIT(AttributedStmt, {})
DEF_STMT_VISIT(BlockExpr, {})
DEF_STMT_VISIT(CoawaitExpr, {})
DEF_STMT_VISIT(CoyieldExpr, {})
DEF_STMT_VISIT(CUDAKernelCallExpr, {})
DEF_STMT_VISIT(CXXUuidofExpr, {})
DEF_STMT_VISIT(ExtVectorElementExpr, {})
DEF_STMT_VISIT(CXXDependentScopeMemberExpr, {})
DEF_STMT_VISIT(CXXTypeidExpr, {})

//--- OpenMP ---------------------------------------------------------------//
// All OMP Stmts don't have any data attached to them,
// so we can just use the default code.
DEF_STMT_VISIT(OMPExecutableDirective, {})
DEF_STMT_VISIT(OMPAtomicDirective, {})
DEF_STMT_VISIT(OMPBarrierDirective, {})
DEF_STMT_VISIT(OMPCancelDirective, {})
DEF_STMT_VISIT(OMPCancellationPointDirective, {})
DEF_STMT_VISIT(OMPCriticalDirective, {})
DEF_STMT_VISIT(OMPFlushDirective, {})
DEF_STMT_VISIT(OMPLoopDirective, {})
DEF_STMT_VISIT(OMPDistributeDirective, {})
DEF_STMT_VISIT(OMPForDirective, {})
DEF_STMT_VISIT(OMPForSimdDirective, {})
DEF_STMT_VISIT(OMPParallelForDirective, {})
DEF_STMT_VISIT(OMPParallelForSimdDirective, {})
DEF_STMT_VISIT(OMPSimdDirective, {})
DEF_STMT_VISIT(OMPTaskLoopDirective, {})
DEF_STMT_VISIT(OMPTaskLoopSimdDirective, {})
DEF_STMT_VISIT(OMPMasterDirective, {})
DEF_STMT_VISIT(OMPOrderedDirective, {})
DEF_STMT_VISIT(OMPParallelDirective, {})
DEF_STMT_VISIT(OMPParallelSectionsDirective, {})
DEF_STMT_VISIT(OMPSectionDirective, {})
DEF_STMT_VISIT(OMPSectionsDirective, {})
DEF_STMT_VISIT(OMPSingleDirective, {})
DEF_STMT_VISIT(OMPTargetDataDirective, {})
DEF_STMT_VISIT(OMPTargetDirective, {})
DEF_STMT_VISIT(OMPTargetEnterDataDirective, {})
DEF_STMT_VISIT(OMPTargetExitDataDirective, {})
DEF_STMT_VISIT(OMPTargetParallelDirective, {})
DEF_STMT_VISIT(OMPTargetParallelForDirective, {})
DEF_STMT_VISIT(OMPTaskDirective, {})
DEF_STMT_VISIT(OMPTaskgroupDirective, {})
DEF_STMT_VISIT(OMPTaskwaitDirective, {})
DEF_STMT_VISIT(OMPTaskyieldDirective, {})
DEF_STMT_VISIT(OMPTeamsDirective, {})
DEF_STMT_VISIT(OMPArraySectionExpr, {})
}

ASTStructure::ASTStructure(ASTContext &Context) {
  StructuralHashVisitor visitor(*this, Context);
  visitor.TraverseDecl(Context.getTranslationUnitDecl());
}


namespace {
class FeatureCollectVisitor
    : public RecursiveASTVisitor<FeatureCollectVisitor> {

public:
  StmtFeature& Feature;

  FeatureCollectVisitor(StmtFeature &Feature) : Feature(Feature) {

  }

  bool VisitNamedDecl(NamedDecl *D) {
    Feature.add(D->getQualifiedNameAsString(), D->getLocStart(), D->getLocEnd(),
                StmtFeature::StmtFeatureKind::NamedDecl);
    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *D) {
    if (auto ND = dyn_cast<NamedDecl>(D->getDecl())) {
      Feature.add(ND->getQualifiedNameAsString(), D->getLocStart(),
                  D->getLocEnd(), StmtFeature::StmtFeatureKind::NamedDecl);
    }
    return true;
  }

  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *E) {
    Feature.add(E->getMethodDecl()->getQualifiedNameAsString(),
                E->getLocStart(), E->getLocEnd(),
                StmtFeature::StmtFeatureKind::FunctionName);
    return true;
  }
};
}

StmtFeature::StmtFeature(StmtInfo Info) {
  FeatureCollectVisitor Visitor(*this);
  if (Info.EndIndex == 0) {
    Visitor.TraverseStmt(Info.S);
  } else {
    for (unsigned I = Info.StartIndex; I < Info.EndIndex; ++I) {
      Visitor.TraverseStmt(static_cast<CompoundStmt*>(Info.S)->body_begin()[I]);
    }
  }
}

void StmtFeature::add(const std::string &Name, SourceLocation StartLoc,
                      SourceLocation EndLoc, StmtFeatureKind Kind) {
  Features[Kind].add(Name, StartLoc, EndLoc);
}

namespace {
class CompareDataVisitor
    : public RecursiveASTVisitor<CompareDataVisitor> {

public:

  std::vector<unsigned> Data;

  bool VisitStmt(Stmt *S) {
    Data.push_back(S->getStmtClass());
    return true;
  }

};

bool CheckStmtEquality(Stmt *S1, Stmt *S2) {

  CompareDataVisitor Visitor1;
  CompareDataVisitor Visitor2;

  Visitor1.TraverseStmt(S1);
  Visitor2.TraverseStmt(S2);

  return Visitor1.Data == Visitor2.Data;
}

void SearchForCloneErrors(std::vector<ASTStructure::CloneMismatch>& output,
                          std::vector<StmtInfo>& Group) {
  for (std::size_t I1 = 0; I1 < Group.size(); ++I1) {
    for (std::size_t I2 = I1 + 1; I2 < Group.size(); ++I2) {
      StmtInfo CurrentStmt = Group[I1];
      StmtInfo OtherStmt = Group[I2];

      if (CurrentStmt.equal(OtherStmt)) {
        StmtFeature CurrentFeature(CurrentStmt);
        StmtFeature OtherFeature(OtherStmt);

        StmtFeature::CompareResult CompareResult =
            CurrentFeature.compare(OtherFeature);

        if (!CompareResult.result.Incompatible &&
            !CompareResult.result.Success) {

          output.push_back(
            ASTStructure::CloneMismatch(
              ASTStructure::CloneInfo(CurrentStmt, OtherStmt),
              CompareResult.result.FeatureThis,
              CompareResult.result.FeatureOther,
              CompareResult.FeaturesThis,
              CompareResult.FeaturesOther,
              CompareResult.MismatchKind
            )
          );
        }
      }
    }
  }
}
}

std::vector<ASTStructure::CloneMismatch> ASTStructure::findCloneErrors() {
  std::vector<ASTStructure::CloneMismatch> result;

  std::map<unsigned, std::vector<StmtInfo> > GroupsByHash;

  for (auto& Pair : HashedStmts) {
    if (Pair.second.Children > 5) {
      GroupsByHash[Pair.second.Hash].push_back(Pair.first);
    }
  }

  for (auto& HashGroupPair : GroupsByHash) {
    if (HashGroupPair.second.size() > 1) {
      SearchForCloneErrors(result, HashGroupPair.second);
    }
  }

  std::set<unsigned> IndexesToRemove;

  for (unsigned I = 0; I < result.size(); ++I) {
    auto Mismatch = result[I];

    for (unsigned J = 0; J < result.size(); ++J) {
      auto OtherMismatch = result[J];
      if (I != J) {
        if (OtherMismatch.Clones.CloneA.contains(Mismatch.Clones.CloneA) &&
            OtherMismatch.Clones.CloneB.contains(Mismatch.Clones.CloneB)) {
          IndexesToRemove.insert(I);
          break;
        }
        if (I < J) {
          if (OtherMismatch.MismatchA.getRange() == Mismatch.MismatchA.getRange()
              || OtherMismatch.MismatchB.getRange() == Mismatch.MismatchB.getRange()
              || OtherMismatch.MismatchA.getRange() == Mismatch.MismatchB.getRange()
              || OtherMismatch.MismatchB.getRange() == Mismatch.MismatchA.getRange()) {
            IndexesToRemove.insert(I);
            break;
          }
        }
      }

    }
  }

  for (auto Iter = IndexesToRemove.rbegin();
       Iter != IndexesToRemove.rend();
       ++Iter) {
    result.erase(result.begin() + (*Iter));
  }

  return result;
}

namespace {
  bool IsChild(Stmt *S, Stmt *PotentialChild) {
    for (Stmt *Child : S->children()) {
      if (Child == PotentialChild) {
        return true;
      } else if (Child != nullptr) {
        if (IsChild(Child, PotentialChild)) {
          return true;
        }
      }
    }
    return false;
  }
}

bool StmtInfo::contains(StmtInfo other) const {
  if (S == other.S) {
    if (EndIndex == 0) {
      return true;
    }
    if (StartIndex <= other.StartIndex && EndIndex >= other.EndIndex) {
      return true;
    }
    return false;
  } else {
    return IsChild(S, other.S);
  }
}

bool StmtInfo::equal(const StmtInfo &other) {
  if (EndIndex != other.EndIndex || StartIndex != other.StartIndex)
    return false;
  if (EndIndex == 0) {
    return CheckStmtEquality(S, other.S);
  } else {
    CompoundStmt *CS = dyn_cast<CompoundStmt>(S);
    CompoundStmt *CSOther = dyn_cast<CompoundStmt>(other.S);
    for (unsigned Index = StartIndex; Index < EndIndex; ++Index) {
      if (!CheckStmtEquality(CS->body_begin()[Index],
                             CSOther->body_begin()[Index]))
        return false;
    }
  }
  return true;
}

void FeatureVector::add(const std::string &FeatureName,
                        SourceLocation StartLocation, SourceLocation EndLocation) {
  for (std::size_t I = 0; I < FeatureNames.size(); ++I) {
    if (FeatureNames[I] == FeatureName) {
      Locations.push_back(Feature(FeatureName, I, StartLocation, EndLocation));
      return;
    }
  }
  Locations.push_back(Feature(FeatureName, FeatureNames.size(),
                              StartLocation, EndLocation));
  FeatureNames.push_back(FeatureName);
}
