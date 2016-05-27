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

    bool PostVisitDecl(Decl *D) {
      finishHash();

      CurrentDecl = D;
      if (D->isImplicit())
        return skip();
      if (IsInAnyMacroBody(Context.getSourceManager(), D->getLocation())) {
        return skip();
      }

      Hash = D->getKind();

      DeclContext *DC = dyn_cast<DeclContext>(D);
      if (DC) {
        for (auto *Child : DC->decls()) {
          if (Child->isImplicit())
            continue;

          calcHash(Child);
        }
      }
      return true;
    }

#define STMT(CLASS, PARENT)                                                    \
  bool PostVisit##CLASS(CLASS *S);
#include "clang/AST/StmtNodes.inc"

#define DECL(CLASS, BASE)                                                      \
  bool PostVisit##CLASS##Decl(CLASS##Decl *D);
#include "clang/AST/DeclNodes.inc"

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

    void calcHash(Decl *D) {
      auto I = SH.findHash(D);
      if (I.Success) {
        calcHash(I.Hash);
      } else {
        skip();
      }
    }

    void calcHash(Stmt *S) {
      auto I = SH.findHash(S);
      if (I.Success) {
        calcHash(I.Hash);
      } else {
        skip();
      }
    }

    void calcHash(QualType QT) {
      const Type *T = QT.getTypePtr();

      if (auto R = T->getAsCXXRecordDecl()) {
        calcHash(R);
      } else if (T->isStructureType() && T->getAsStructureType()->getDecl()) {
        auto R = T->getAsStructureType()->getDecl();
        calcHash(R);
      } else if (T->isBuiltinType()) {
        calcHash(786);
      } else {
        QT->dump();
        assert(false);
      }
    }

    void finishHash() {
      if (SkipHash) {
        CurrentStmt = nullptr;
        CurrentDecl = nullptr;
        SkipHash = false;
      }
      if (CurrentStmt) {
        Hash += ClassHash;
        SH.add(Hash, CurrentStmt);
        CurrentStmt = nullptr;
      } else if (CurrentDecl) {
        SH.add(Hash, CurrentDecl);
        CurrentDecl = nullptr;
      }
    }

    // If the current hash should not be saved for later use.
    bool SkipHash = false;
    // The hash value that is calculated right now.
    unsigned Hash;
    unsigned ClassHash;
    // The current statement that is being hashed at the moment
    // or 0 if there is no statement currently hashed.
    Stmt *CurrentStmt = nullptr;
    // The current declaration that is being hashed at the moment
    // or 0 if there is no declaration currently hashed.
    Decl *CurrentDecl = nullptr;

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

#define DEF_DECL_VISIT(CLASS, CODE)                                            \
  bool StructuralHashVisitor::PostVisit##CLASS(CLASS *D)                       \
  {                                                                            \
    if (SkipHash)                                                              \
      return true;                                                             \
    { CODE; }                                                                  \
    return true;                                                               \
  }

  // TODO: misc stmts
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

  // TODO Obj-C support
  DEF_STMT_VISIT(ObjCArrayLiteral, {})
  DEF_STMT_VISIT(ObjCBoxedExpr, {})
  DEF_STMT_VISIT(ObjCDictionaryLiteral, {})
  DEF_STMT_VISIT(ObjCEncodeExpr, {})
  DEF_STMT_VISIT(ObjCIndirectCopyRestoreExpr, {})
  DEF_STMT_VISIT(ObjCIsaExpr, {})
  DEF_STMT_VISIT(ObjCIvarRefExpr, {})
  DEF_STMT_VISIT(ObjCMessageExpr, {})
  DEF_STMT_VISIT(ObjCPropertyRefExpr, {})
  DEF_STMT_VISIT(ObjCProtocolExpr, {})
  DEF_STMT_VISIT(ObjCSelectorExpr, {})
  DEF_STMT_VISIT(ObjCStringLiteral, {})
  DEF_STMT_VISIT(ObjCSubscriptRefExpr, {})
  DEF_STMT_VISIT(ObjCAtCatchStmt, {})
  DEF_STMT_VISIT(ObjCAtFinallyStmt, {})
  DEF_STMT_VISIT(ObjCAtSynchronizedStmt, {})
  DEF_STMT_VISIT(ObjCAtThrowStmt, {})
  DEF_STMT_VISIT(ObjCAtTryStmt, {})
  DEF_STMT_VISIT(ObjCAutoreleasePoolStmt, {})
  DEF_STMT_VISIT(ObjCForCollectionStmt, {})

  // TODO OpenMP support
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


  // No special hashing needed
  DEF_STMT_VISIT(BreakStmt, {})
  DEF_STMT_VISIT(CXXTryStmt, {})
  DEF_STMT_VISIT(CompoundStmt, {})
  DEF_STMT_VISIT(ContinueStmt, {})
  DEF_STMT_VISIT(DoStmt, {})
  DEF_STMT_VISIT(Expr, {})
  DEF_STMT_VISIT(AddrLabelExpr, {})
  DEF_STMT_VISIT(ArrayTypeTraitExpr, {})
  DEF_STMT_VISIT(CompoundAssignOperator, {})
  DEF_STMT_VISIT(CXXBindTemporaryExpr, {})
  DEF_STMT_VISIT(ReturnStmt, {})
  DEF_STMT_VISIT(NullStmt, {})
  DEF_STMT_VISIT(GNUNullExpr, {})
  DEF_STMT_VISIT(CXXNewExpr, {})
  DEF_STMT_VISIT(CXXNoexceptExpr, {})
  DEF_STMT_VISIT(CXXNullPtrLiteralExpr, {})
  DEF_STMT_VISIT(CXXScalarValueInitExpr, {})
  DEF_STMT_VISIT(CXXThisExpr, {})
  DEF_STMT_VISIT(CXXFunctionalCastExpr, {})
  DEF_STMT_VISIT(CXXNamedCastExpr, {})
  DEF_STMT_VISIT(CXXConstCastExpr, {})
  DEF_STMT_VISIT(CXXDynamicCastExpr, {})
  DEF_STMT_VISIT(CXXReinterpretCastExpr, {})
  DEF_STMT_VISIT(CXXStaticCastExpr, {})


  DEF_STMT_VISIT(CXXCatchStmt, {
                   if (S->getExceptionDecl())
                     calcHash(S->getExceptionDecl());
                   else
                     calcHash(356);

                   if (S->getCaughtType().getTypePtrOrNull())
                     calcHash(S->getCaughtType());
                   else
                     calcHash(265);
                 })

  DEF_STMT_VISIT(DeclStmt, {})
  DEF_STMT_VISIT(AbstractConditionalOperator, {})
  DEF_STMT_VISIT(BinaryConditionalOperator, {})
  DEF_STMT_VISIT(ConditionalOperator, {})
  DEF_STMT_VISIT(ArraySubscriptExpr, {})
  DEF_STMT_VISIT(AsTypeExpr, {})
  DEF_STMT_VISIT(AtomicExpr, {
                   calcHash(S->isVolatile());
                   calcHash(S->getOp());
                 })
  DEF_STMT_VISIT(BinaryOperator, {
                   calcHash(S->getOpcode());
                 })
  DEF_STMT_VISIT(CXXConstructExpr, {})
  DEF_STMT_VISIT(CXXTemporaryObjectExpr, {})
  DEF_STMT_VISIT(CXXDefaultArgExpr, {})
  DEF_STMT_VISIT(CXXDefaultInitExpr, {})
  DEF_STMT_VISIT(CXXDeleteExpr, {
                   calcHash(S->isArrayFormAsWritten());
                   calcHash(S->isGlobalDelete());
                 })
  DEF_STMT_VISIT(CXXDependentScopeMemberExpr, {})
  DEF_STMT_VISIT(CXXFoldExpr, {
                   calcHash(S->isRightFold());
                   calcHash(S->getOperator());
                 })
  DEF_STMT_VISIT(CXXPseudoDestructorExpr, {
                   S->isArrow();
                 })
  DEF_STMT_VISIT(CXXStdInitializerListExpr, {})
  DEF_STMT_VISIT(CXXThrowExpr, {})
  DEF_STMT_VISIT(CXXTypeidExpr, {})
  DEF_STMT_VISIT(CXXUnresolvedConstructExpr, {})
  DEF_STMT_VISIT(CallExpr, {})
  DEF_STMT_VISIT(CXXMemberCallExpr, {})
  DEF_STMT_VISIT(CXXOperatorCallExpr, {})
  DEF_STMT_VISIT(UserDefinedLiteral, {})
  DEF_STMT_VISIT(CastExpr, {})
  DEF_STMT_VISIT(ExplicitCastExpr, {})
  DEF_STMT_VISIT(CStyleCastExpr, {})
  DEF_STMT_VISIT(ObjCBridgedCastExpr, {})
  DEF_STMT_VISIT(ImplicitCastExpr, {})
  DEF_STMT_VISIT(ChooseExpr, {})
  DEF_STMT_VISIT(CompoundLiteralExpr, {})
  DEF_STMT_VISIT(ConvertVectorExpr, {})
  DEF_STMT_VISIT(DeclRefExpr, {})
  DEF_STMT_VISIT(DependentScopeDeclRefExpr, {})
  DEF_STMT_VISIT(DesignatedInitExpr, {})
  DEF_STMT_VISIT(DesignatedInitUpdateExpr, {})
  DEF_STMT_VISIT(ExprWithCleanups, {})
  DEF_STMT_VISIT(ExpressionTraitExpr, {})
  DEF_STMT_VISIT(ExtVectorElementExpr, {})
  DEF_STMT_VISIT(FunctionParmPackExpr, {})
  DEF_STMT_VISIT(GenericSelectionExpr, {})


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

  DEF_STMT_VISIT(ObjCBoolLiteralExpr, {})
  DEF_STMT_VISIT(CXXBoolLiteralExpr, {})

  DEF_STMT_VISIT(ImplicitValueInitExpr, {})
  DEF_STMT_VISIT(InitListExpr, {})
  DEF_STMT_VISIT(IntegerLiteral, {})
  DEF_STMT_VISIT(LambdaExpr, {})
  DEF_STMT_VISIT(MSPropertyRefExpr, {})
  DEF_STMT_VISIT(MSPropertySubscriptExpr, {})
  DEF_STMT_VISIT(MaterializeTemporaryExpr, {})
  DEF_STMT_VISIT(MemberExpr, {})
  DEF_STMT_VISIT(NoInitExpr, {})
  DEF_STMT_VISIT(OffsetOfExpr, {})
  DEF_STMT_VISIT(OpaqueValueExpr, {})
  DEF_STMT_VISIT(OverloadExpr, {})
  DEF_STMT_VISIT(UnresolvedLookupExpr, {})
  DEF_STMT_VISIT(UnresolvedMemberExpr, {})
  DEF_STMT_VISIT(PackExpansionExpr, {})
  DEF_STMT_VISIT(ParenExpr, {})
  DEF_STMT_VISIT(ParenListExpr, {})
  DEF_STMT_VISIT(PredefinedExpr, {})
  DEF_STMT_VISIT(PseudoObjectExpr, {})
  DEF_STMT_VISIT(ShuffleVectorExpr, {})
  DEF_STMT_VISIT(SizeOfPackExpr, {})
  DEF_STMT_VISIT(StmtExpr, {})
  DEF_STMT_VISIT(StringLiteral, {})
  DEF_STMT_VISIT(SubstNonTypeTemplateParmExpr, {})
  DEF_STMT_VISIT(SubstNonTypeTemplateParmPackExpr, {})
  DEF_STMT_VISIT(TypeTraitExpr, {})
  DEF_STMT_VISIT(TypoExpr, {})
  DEF_STMT_VISIT(UnaryExprOrTypeTraitExpr, {})
  DEF_STMT_VISIT(UnaryOperator, {
                   calcHash(S->getOpcode());
                 })
  DEF_STMT_VISIT(VAArgExpr, {})
  DEF_STMT_VISIT(ForStmt, {
                   if (S->getConditionVariable())
                     calcHash(S->getConditionVariable());
                 })
  DEF_STMT_VISIT(GotoStmt, {})
  DEF_STMT_VISIT(IfStmt, {
                   if (S->getConditionVariable())
                     calcHash(S->getConditionVariable());
                 })
  DEF_STMT_VISIT(IndirectGotoStmt, {})
  DEF_STMT_VISIT(LabelStmt, {
                   //TODO calcHash(S->getDecl()->getName());
                 })
  DEF_STMT_VISIT(MSDependentExistsStmt, {
                   calcHash(S->isIfExists());
                 })

  DEF_STMT_VISIT(SEHExceptStmt, {})
  DEF_STMT_VISIT(SEHFinallyStmt, {})
  DEF_STMT_VISIT(SEHLeaveStmt, {})
  DEF_STMT_VISIT(SEHTryStmt, {})

  DEF_STMT_VISIT(SwitchCase, {})
  DEF_STMT_VISIT(CaseStmt, {})
  DEF_STMT_VISIT(DefaultStmt, {})
  DEF_STMT_VISIT(SwitchStmt, {
                   if (S->getConditionVariable())
                     calcHash(S->getConditionVariable());
                 })
  DEF_STMT_VISIT(WhileStmt, {
                   if (S->getConditionVariable())
                     calcHash(S->getConditionVariable());

                 })

  // Declarations

  // TODO: misc declarations
  DEF_DECL_VISIT(BlockDecl, {})

  // TODO: Obj-C support
  DEF_DECL_VISIT(ObjCCompatibleAliasDecl, {})
  DEF_DECL_VISIT(ObjCContainerDecl, {})
  DEF_DECL_VISIT(ObjCCategoryDecl, {})
  DEF_DECL_VISIT(ObjCImplDecl, {})
  DEF_DECL_VISIT(ObjCCategoryImplDecl, {})
  DEF_DECL_VISIT(ObjCImplementationDecl, {})
  DEF_DECL_VISIT(ObjCInterfaceDecl, {})
  DEF_DECL_VISIT(ObjCProtocolDecl, {})
  DEF_DECL_VISIT(ObjCMethodDecl, {})
  DEF_DECL_VISIT(ObjCPropertyDecl, {})
  DEF_DECL_VISIT(ObjCTypeParamDecl, {})
  DEF_DECL_VISIT(ObjCAtDefsFieldDecl, {})
  DEF_DECL_VISIT(ObjCIvarDecl, {})
  DEF_DECL_VISIT(ObjCPropertyImplDecl, {})

  // TODO: OpenMP support
  DEF_DECL_VISIT(OMPCapturedExprDecl, {})
  DEF_DECL_VISIT(OMPDeclareReductionDecl, {})
  DEF_DECL_VISIT(OMPThreadPrivateDecl, {})


  DEF_DECL_VISIT(AccessSpecDecl, {})
  DEF_DECL_VISIT(CapturedDecl, {})
  DEF_DECL_VISIT(ClassScopeFunctionSpecializationDecl, {})
  DEF_DECL_VISIT(EmptyDecl, {})
  DEF_DECL_VISIT(ExternCContextDecl, {})
  DEF_DECL_VISIT(FileScopeAsmDecl, {})
  DEF_DECL_VISIT(FriendDecl, {})
  DEF_DECL_VISIT(FriendTemplateDecl, {})
  DEF_DECL_VISIT(ImportDecl, {})
  DEF_DECL_VISIT(LinkageSpecDecl, {})
  DEF_DECL_VISIT(NamedDecl, {})
  DEF_DECL_VISIT(LabelDecl, {
                   calcHash(D->isMSAsmLabel());
                   calcHash(D->isGnuLocal());
                   calcHash(D->isResolvedMSAsmLabel());
                   //if (D->isMSAsmLabel())
                   //  calcHash(D->getMSAsmLabel());
                 })
  DEF_DECL_VISIT(NamespaceDecl, {})
  DEF_DECL_VISIT(NamespaceAliasDecl, {})
  DEF_DECL_VISIT(TemplateDecl, {
                   for (auto T : *D->getTemplateParameters()) {
                     calcHash(T);
                   }
                 })
  DEF_DECL_VISIT(BuiltinTemplateDecl, {})
  DEF_DECL_VISIT(RedeclarableTemplateDecl, {})
  DEF_DECL_VISIT(ClassTemplateDecl, {})
  DEF_DECL_VISIT(FunctionTemplateDecl, {
                   calcHash(D->getTemplatedDecl());
                 })
  DEF_DECL_VISIT(TypeAliasTemplateDecl, {})
  DEF_DECL_VISIT(VarTemplateDecl, {})
  DEF_DECL_VISIT(TemplateTemplateParmDecl, {})
  DEF_DECL_VISIT(TypeDecl, {})
  DEF_DECL_VISIT(TagDecl, {})
  DEF_DECL_VISIT(EnumDecl, {
                   calcHash(D->isScoped());
                   calcHash(D->isFixed());
                   calcHash(D->isScopedUsingClassTag());
                 })
  DEF_DECL_VISIT(RecordDecl, {
                   calcHash(D->isMsStruct(Context));
                   calcHash(D->isLambda());
                   calcHash(D->isCapturedRecord());
                   calcHash(D->isAnonymousStructOrUnion());
                   calcHash(D->isInjectedClassName());
                 })
  DEF_DECL_VISIT(CXXRecordDecl, {
                   for (CXXBaseSpecifier& Base : D->bases()) {
                     calcHash(23);
                     calcHash(Base.getAccessSpecifierAsWritten());
                     calcHash(Base.getType());
                   }
                   for (CXXBaseSpecifier& Base : D->bases()) {
                     calcHash(7);
                     calcHash(Base.getAccessSpecifierAsWritten());
                     calcHash(Base.getType());
                   }
                 })
  DEF_DECL_VISIT(ClassTemplateSpecializationDecl, {})
  DEF_DECL_VISIT(ClassTemplatePartialSpecializationDecl, {})
  DEF_DECL_VISIT(TemplateTypeParmDecl, {
                   // TODO hash whether 'class' or 'typename'
                   // parameter
                   calcHash(D->wasDeclaredWithTypename());
                 })
  DEF_DECL_VISIT(TypedefNameDecl, {})
  DEF_DECL_VISIT(TypeAliasDecl, {})
  DEF_DECL_VISIT(TypedefDecl, {})
  DEF_DECL_VISIT(UnresolvedUsingTypenameDecl, {})
  DEF_DECL_VISIT(UsingDecl, {})
  DEF_DECL_VISIT(UsingDirectiveDecl, {})
  DEF_DECL_VISIT(UsingShadowDecl, {})
  DEF_DECL_VISIT(ValueDecl, {})
  DEF_DECL_VISIT(DeclaratorDecl, {})
  DEF_DECL_VISIT(FieldDecl, {})
  DEF_DECL_VISIT(FunctionDecl, {
                   calcHash(D->isVariadic());
                   if (D->hasBody()) {
                     calcHash(D->getBody());
                   }
                 })
  DEF_DECL_VISIT(CXXMethodDecl, {
                   calcHash(D->isStatic());
                   calcHash(D->isConst());
                   calcHash(D->isVolatile());
                   calcHash(D->isVirtualAsWritten());
                 })
  DEF_DECL_VISIT(CXXConstructorDecl, {})
  DEF_DECL_VISIT(CXXConversionDecl, {})
  DEF_DECL_VISIT(CXXDestructorDecl, {
                   calcHash(D->isVirtualAsWritten());
                 })
  DEF_DECL_VISIT(MSPropertyDecl, {})
  DEF_DECL_VISIT(NonTypeTemplateParmDecl, {})
  DEF_DECL_VISIT(VarDecl, {})
  DEF_DECL_VISIT(ImplicitParamDecl, {})
  DEF_DECL_VISIT(ParmVarDecl, {})
  DEF_DECL_VISIT(VarTemplateSpecializationDecl, {})
  DEF_DECL_VISIT(VarTemplatePartialSpecializationDecl, {})
  DEF_DECL_VISIT(EnumConstantDecl, {})
  DEF_DECL_VISIT(IndirectFieldDecl, {})
  DEF_DECL_VISIT(UnresolvedUsingValueDecl, {})
  DEF_DECL_VISIT(PragmaCommentDecl, {})
  DEF_DECL_VISIT(PragmaDetectMismatchDecl, {})
  DEF_DECL_VISIT(StaticAssertDecl, {})
  DEF_DECL_VISIT(TranslationUnitDecl, {})
}

ASTStructure::ASTStructure(ASTContext& Context) {
  StructuralHashVisitor visitor(*this, Context);
  visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

