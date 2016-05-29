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
    explicit StructuralHashVisitor(ASTStructure& Hash, ASTContext& Context)
      : SH(Hash), Context(Context)
    {
    }

    virtual ~StructuralHashVisitor() {
      finishHash();
    }

    bool shouldTraversePostOrder() const { return true; }

    // TODO this code is ironically copy-pasted
    // Returns true if the SourceLocation is expanded from any macro body.
    // Returns false if the SourceLocation is invalid, is from not in a macro
    // expansion, or is from expanded from a top-level macro argument.
    static bool IsInAnyMacroBody(const SourceManager &SM, SourceLocation Loc) {
      if (Loc.isInvalid())
        return false;

      while (Loc.isMacroID()) {
        if (SM.isMacroBodyExpansion(Loc))
          return true;
        Loc = SM.getImmediateMacroCallerLoc(Loc);
      }

      return false;
    }

    bool PostVisitStmt(Stmt *S) {
      finishHash();

      CurrentStmt = S;
      Hash = 0;
      ClassHash = S->getStmtClass();

      if (IsInAnyMacroBody(Context.getSourceManager(), S->getLocStart())) {
        switch (S->getStmtClass()) {
          case Stmt::FloatingLiteralClass:
          case Stmt::CXXBoolLiteralExprClass:
          case Stmt::IntegerLiteralClass:
            break;
          default:
            return skip();
          }
      }

      for (Stmt *Child : S->children()) {
        if (Child == nullptr) {
            calcHash(251);
          } else if (Child->getSourceRange().isInvalid()) {
            calcHash(569);
          } else {
            calcHash(Child);
          }
      }

      return true;
    }

#define STMT(CLASS, PARENT)                                                    \
  bool PostVisit##CLASS(CLASS *S);
#include "clang/AST/StmtNodes.inc"

  private:

    bool skip() {
      SkipHash = true;
      return true;
    }

    void calcHash(unsigned Value) {
      Hash *= 53;
      Hash += Value;
    }

    void calcHash(const llvm::StringRef& String) {
      for (char c : String) {
        calcHash(static_cast<unsigned>(c));
      }
    }

    void calcHash(Stmt *S) {
      auto I = SH.findHash(S);
      if (I.Success) {
        calcHash(I.Hash);
      }
    }

    void finishHash() {
      if (SkipHash) {
        CurrentStmt = nullptr;
        SkipHash = false;
      }
      if (!IgnoreClassHash) {
        Hash += ClassHash;
      }
      IgnoreClassHash = false;
      SH.add(Hash, CurrentStmt);
      CurrentStmt = nullptr;
    }

    // If the current hash should not be saved for later use.
    bool SkipHash = false;
    // The hash value that is calculated right now.
    unsigned Hash;
    bool IgnoreClassHash = false;
    unsigned ClassHash;
    // The current statement that is being hashed at the moment
    // or 0 if there is no statement currently hashed.
    Stmt *CurrentStmt = nullptr;

    ASTStructure& SH;
    ASTContext& Context;
  };

#define DEF_STMT_VISIT(CLASS, CODE)                                            \
  bool StructuralHashVisitor::PostVisit##CLASS(CLASS *S)                       \
  {                                                                            \
    if (SkipHash)                                                              \
      return true;                                                             \
    { CODE; }                                                                  \
    return true;                                                               \
  }

  // Builtins
  DEF_STMT_VISIT(ArrayTypeTraitExpr, {
                   calcHash(S->getTrait());
                 })
  DEF_STMT_VISIT(AsTypeExpr, {})
  DEF_STMT_VISIT(AtomicExpr, {
                   calcHash(S->isVolatile());
                   calcHash(S->getOp());
                 })
  DEF_STMT_VISIT(ChooseExpr, {})
  DEF_STMT_VISIT(ConvertVectorExpr, {})
  DEF_STMT_VISIT(CXXNoexceptExpr, {})
  DEF_STMT_VISIT(ExpressionTraitExpr, {
                   calcHash(S->getTrait());
                 })
  DEF_STMT_VISIT(ShuffleVectorExpr, {})
  DEF_STMT_VISIT(PredefinedExpr, {
                   calcHash(S->getIdentType());
                 })
  DEF_STMT_VISIT(TypeTraitExpr, {
                   calcHash(S->getTrait());
                 })
  DEF_STMT_VISIT(VAArgExpr, {})

  // Misc
  DEF_STMT_VISIT(CXXFoldExpr, {
                   calcHash(S->isRightFold());
                   calcHash(S->getOperator());
                 })
  DEF_STMT_VISIT(StmtExpr, {})
  DEF_STMT_VISIT(ExprWithCleanups, {})
  DEF_STMT_VISIT(GenericSelectionExpr, {
                   calcHash(S->getNumAssocs());
                 })
  DEF_STMT_VISIT(LambdaExpr, {
                   for (const LambdaCapture& C : S->captures()) {
                     calcHash(C.isPackExpansion());
                     calcHash(C.getCaptureKind());
                   }
                   calcHash(S->isGenericLambda());
                   calcHash(S->isMutable());
                   calcHash(S->getCallOperator()->param_size());
                 })
  DEF_STMT_VISIT(OpaqueValueExpr, {
                   IgnoreClassHash = true;
                 })
  DEF_STMT_VISIT(MaterializeTemporaryExpr, {
                   IgnoreClassHash = true;
                 })
  DEF_STMT_VISIT(SubstNonTypeTemplateParmExpr, {})
  DEF_STMT_VISIT(SubstNonTypeTemplateParmPackExpr, {})
  DEF_STMT_VISIT(DeclStmt, {
                   auto numDecls = std::distance(S->decl_begin(),
                                                        S->decl_end());
                   calcHash(537u + static_cast<unsigned>(numDecls));
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
  DEF_STMT_VISIT(AsmStmt, {})
  DEF_STMT_VISIT(GCCAsmStmt, {
                   calcHash(S->isVolatile());
                   calcHash(S->isSimple());
                   calcHash(S->getAsmString());
                   calcHash(S->getNumOutputs());
                   for (unsigned I = 0, N = S->getNumOutputs(); I != N; ++I) {
                     calcHash(S->getOutputName(I));
                     VisitStringLiteral(S->getOutputConstraintLiteral(I));
                   }
                   calcHash(S->getNumInputs());
                   for (unsigned I = 0, N = S->getNumInputs(); I != N; ++I) {
                     calcHash(S->getInputName(I));
                     calcHash(S->getInputConstraintLiteral(I));
                   }
                   calcHash(S->getNumClobbers());
                   for (unsigned I = 0, N = S->getNumClobbers(); I != N; ++I)
                     calcHash(S->getClobberStringLiteral(I));})
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
  DEF_STMT_VISIT(PseudoObjectExpr, {})


  // MS properties
  DEF_STMT_VISIT(MSPropertyRefExpr, {})
  DEF_STMT_VISIT(MSPropertySubscriptExpr, {})

  // Calls
  DEF_STMT_VISIT(CXXOperatorCallExpr, {
                   calcHash(S->getOperator());
                 })
  DEF_STMT_VISIT(CXXMemberCallExpr, {})
  DEF_STMT_VISIT(CallExpr, {})

  // Invalid code
  DEF_STMT_VISIT(TypoExpr, {
                   return skip();
                 })
  DEF_STMT_VISIT(UnresolvedLookupExpr, {
                   return skip();
                 })
  DEF_STMT_VISIT(UnresolvedMemberExpr, {
                   return skip();
                 })
  DEF_STMT_VISIT(CXXUnresolvedConstructExpr, {
                   return skip();
                 })
  DEF_STMT_VISIT(OverloadExpr, {
                   return skip();
                 })

  // Exceptions
  DEF_STMT_VISIT(CXXThrowExpr, {})
  DEF_STMT_VISIT(CXXCatchStmt, {
                   if (S->getExceptionDecl())
                     calcHash(355);
                 })
  DEF_STMT_VISIT(CXXTryStmt, {})
  DEF_STMT_VISIT(SEHExceptStmt, {})
  DEF_STMT_VISIT(SEHFinallyStmt, {})
  DEF_STMT_VISIT(SEHLeaveStmt, {})
  DEF_STMT_VISIT(SEHTryStmt, {})

  // Literals
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

  // TODO
  DEF_STMT_VISIT(UserDefinedLiteral, {})

  // OOP
  DEF_STMT_VISIT(MemberExpr, {})
  DEF_STMT_VISIT(CXXNewExpr, {})
  DEF_STMT_VISIT(CXXThisExpr, {})
  DEF_STMT_VISIT(CXXConstructExpr, {})
  DEF_STMT_VISIT(CXXDeleteExpr, {
                   calcHash(S->isArrayFormAsWritten());
                   calcHash(S->isGlobalDelete());
                 })
  DEF_STMT_VISIT(DesignatedInitExpr, {})
  DEF_STMT_VISIT(DesignatedInitUpdateExpr, {})
  DEF_STMT_VISIT(NoInitExpr, {})
  DEF_STMT_VISIT(InitListExpr, {})
  DEF_STMT_VISIT(CXXStdInitializerListExpr, {})

  DEF_STMT_VISIT(CXXTemporaryObjectExpr, {
                   IgnoreClassHash = true;
                 })
  DEF_STMT_VISIT(CXXDefaultArgExpr, {})
  DEF_STMT_VISIT(CXXDefaultInitExpr, {})

  // Casts
  DEF_STMT_VISIT(CXXFunctionalCastExpr, {})

  DEF_STMT_VISIT(CXXNamedCastExpr, {})
  DEF_STMT_VISIT(CXXConstCastExpr, {})
  DEF_STMT_VISIT(CXXDynamicCastExpr, {})
  DEF_STMT_VISIT(CXXReinterpretCastExpr, {})
  DEF_STMT_VISIT(CXXStaticCastExpr, {})

  DEF_STMT_VISIT(ImplicitCastExpr, {
                   IgnoreClassHash = true;
                 })

  DEF_STMT_VISIT(CastExpr, {})
  DEF_STMT_VISIT(ExplicitCastExpr, {})
  DEF_STMT_VISIT(CStyleCastExpr, {})
  DEF_STMT_VISIT(ObjCBridgedCastExpr, {
                   calcHash(S->getBridgeKind());
                 })

  // Exprs
  DEF_STMT_VISIT(Expr, {})
  DEF_STMT_VISIT(ParenExpr, {})
  DEF_STMT_VISIT(ArraySubscriptExpr, {})
  DEF_STMT_VISIT(BinaryOperator, {
                   calcHash(S->getOpcode());
                 })
  DEF_STMT_VISIT(UnaryOperator, {
                   calcHash(S->getOpcode());
                 })

  // Control flow
  DEF_STMT_VISIT(ForStmt, {})
  DEF_STMT_VISIT(GotoStmt, {})
  DEF_STMT_VISIT(IfStmt, {})
  DEF_STMT_VISIT(WhileStmt, {})
  DEF_STMT_VISIT(IndirectGotoStmt, {})
  DEF_STMT_VISIT(LabelStmt, {
                   calcHash(S->getDecl()->getName());
                 })
  DEF_STMT_VISIT(MSDependentExistsStmt, {
                   calcHash(S->isIfExists());
                 })
  DEF_STMT_VISIT(AddrLabelExpr, {
                   calcHash(S->getLabel()->getName());
                 })
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


  // Objective C
  DEF_STMT_VISIT(ObjCArrayLiteral, {})
  DEF_STMT_VISIT(ObjCBoxedExpr, {})
  DEF_STMT_VISIT(ObjCDictionaryLiteral, {})
  DEF_STMT_VISIT(ObjCEncodeExpr, {})
  DEF_STMT_VISIT(ObjCIndirectCopyRestoreExpr, {
                   calcHash(S->shouldCopy());
                 })
  DEF_STMT_VISIT(ObjCIsaExpr, {})
  DEF_STMT_VISIT(ObjCIvarRefExpr, {})
  DEF_STMT_VISIT(ObjCMessageExpr, {})
  DEF_STMT_VISIT(ObjCPropertyRefExpr, {
                   calcHash(S->isSuperReceiver());
                   calcHash(S->isImplicitProperty());
                 })
  DEF_STMT_VISIT(ObjCProtocolExpr, {})
  DEF_STMT_VISIT(ObjCSelectorExpr, {})
  DEF_STMT_VISIT(ObjCSubscriptRefExpr, {})
  DEF_STMT_VISIT(ObjCAtCatchStmt, {
                   calcHash(S->hasEllipsis());
                 })
  DEF_STMT_VISIT(ObjCAtFinallyStmt, {})
  DEF_STMT_VISIT(ObjCAtSynchronizedStmt, {})
  DEF_STMT_VISIT(ObjCAtThrowStmt, {})
  DEF_STMT_VISIT(ObjCAtTryStmt, {})
  DEF_STMT_VISIT(ObjCAutoreleasePoolStmt, {})
  DEF_STMT_VISIT(ObjCForCollectionStmt, {})


  // OpenMP
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

ASTStructure::ASTStructure(ASTContext& Context) {
  StructuralHashVisitor visitor(*this, Context);
  visitor.TraverseDecl(Context.getTranslationUnitDecl());
}
