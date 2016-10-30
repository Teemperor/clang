//===--- CloneDetection.h - Finds code clones in an AST ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// /file
/// This file defines classes for searching and anlyzing source code clones.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_CLONEDETECTION_H
#define LLVM_CLANG_AST_CLONEDETECTION_H

#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/Hashing.h"

#include <vector>

namespace clang {

class Stmt;
class Decl;
class VarDecl;
class ASTContext;
class CompoundStmt;

/// \brief Identifies a list of statements.
///
/// Can either identify a single arbitrary Stmt object, a continuous sequence of
/// child statements inside a CompoundStmt or no statements at all.
class StmtSequence {
  /// If this object identifies a sequence of statements inside a CompoundStmt,
  /// S points to this CompoundStmt. If this object only identifies a single
  /// Stmt, then S is a pointer to this Stmt.
  const Stmt *S;

  /// The related ASTContext for S.
  ASTContext *Context;

  /// If EndIndex is non-zero, then S is a CompoundStmt and this StmtSequence
  /// instance is representing the CompoundStmt children inside the array
  /// [StartIndex, EndIndex).
  unsigned StartIndex;
  unsigned EndIndex;

public:
  /// \brief Constructs a StmtSequence holding multiple statements.
  ///
  /// The resulting StmtSequence identifies a continuous sequence of statements
  /// in the body of the given CompoundStmt. Which statements of the body should
  /// be identified needs to be specified by providing a start and end index
  /// that describe a non-empty sub-array in the body of the given CompoundStmt.
  ///
  /// \param Stmt A CompoundStmt that contains all statements in its body.
  /// \param Context The ASTContext for the given CompoundStmt.
  /// \param StartIndex The inclusive start index in the children array of
  ///                   \p Stmt
  /// \param EndIndex The exclusive end index in the children array of \p Stmt.
  StmtSequence(const CompoundStmt *Stmt, ASTContext &Context,
               unsigned StartIndex, unsigned EndIndex);

  /// \brief Constructs a StmtSequence holding a single statement.
  ///
  /// \param Stmt An arbitrary Stmt.
  /// \param Context The ASTContext for the given Stmt.
  StmtSequence(const Stmt *Stmt, ASTContext &Context);

  /// \brief Constructs an empty StmtSequence.
  StmtSequence();

  typedef const Stmt *const *iterator;

  /// Returns an iterator pointing to the first statement in this sequence.
  iterator begin() const;

  /// Returns an iterator pointing behind the last statement in this sequence.
  iterator end() const;

  /// Returns the first statement in this sequence.
  ///
  /// This method should only be called on a non-empty StmtSequence object.
  const Stmt *front() const {
    assert(!empty());
    return begin()[0];
  }

  /// Returns the last statement in this sequence.
  ///
  /// This method should only be called on a non-empty StmtSequence object.
  const Stmt *back() const {
    assert(!empty());
    return begin()[size() - 1];
  }

  /// Returns the number of statements this object holds.
  unsigned size() const {
    if (holdsSequence())
      return EndIndex - StartIndex;
    if (S == nullptr)
      return 0;
    return 1;
  }

  /// Returns true if and only if this StmtSequence contains no statements.
  bool empty() const { return size() == 0; }

  /// Returns the related ASTContext for the stored Stmts.
  ASTContext &getASTContext() const {
    assert(Context);
    return *Context;
  }

  /// Returns true if this objects holds a list of statements.
  bool holdsSequence() const { return EndIndex != 0; }

  /// Returns the start sourcelocation of the first statement in this sequence.
  ///
  /// This method should only be called on a non-empty StmtSequence object.
  SourceLocation getStartLoc() const;

  /// Returns the end sourcelocation of the last statement in this sequence.
  ///
  /// This method should only be called on a non-empty StmtSequence object.
  SourceLocation getEndLoc() const;

  bool operator==(const StmtSequence &Other) const {
    return std::tie(S, StartIndex, EndIndex) ==
           std::tie(Other.S, Other.StartIndex, Other.EndIndex);
  }

  bool operator!=(const StmtSequence &Other) const {
    return std::tie(S, StartIndex, EndIndex) !=
           std::tie(Other.S, Other.StartIndex, Other.EndIndex);
  }

  /// Returns true if and only if this sequence covers a source range that
  /// contains the source range of the given sequence \p Other.
  ///
  /// This method should only be called on a non-empty StmtSequence object
  /// and passed a non-empty StmtSequence object.
  bool contains(const StmtSequence &Other) const;
};

/// \brief Searches for similar subtrees in the AST.
///
/// First, this class needs several declarations with statement bodies which
/// can be passed via analyzeCodeBody. Afterwards all statements can be
/// searched for clones by calling findClones with a given list of constraints
/// that should specify the wanted properties of the clones.
///
/// The result of findClones can be further constrained with the constrainClones
/// method.
///
/// This class only searches for clones in exectuable source code
/// (e.g. function bodies). Other clones (e.g. cloned comments or declarations)
/// are not supported.
class CloneDetector {

public:
  /// \brief A collection of StmtSequences that share an arbitrary property.
  typedef llvm::SmallVector<StmtSequence, 8> CloneGroup;

  /// \brief Generates and stores search data for all statements in the body of
  ///        the given Decl.
  void analyzeCodeBody(const Decl *D);

  template <typename T>
  void constrainClones(std::vector<CloneGroup> &CloneGroups, T C) {
    C.constrain(CloneGroups);
  }

  /// \brief Constrains the given list of clone groups
  template <typename T1, typename... Ts>
  void constrainClones(std::vector<CloneGroup> &CloneGroups, T1 C,
                       Ts... ConstraintList) {
    C.constrain(CloneGroups);
    constrainClones(CloneGroups, ConstraintList...);
  }

  /// \brief Searches for clones in all previously passed statements.
  /// \param Result Output parameter to which all created clone groups are
  ///               added.
  /// \param Passes The constraints that should be applied to the result.
  template <typename... Ts>
  void findClones(std::vector<CloneGroup> &Result, Ts... ConstraintList) {
    // The initial assumption is that there is only one clone group and every
    // statement is a clone of the others. This clone group will then be
    // split up with the help of the constraints.
    CloneGroup AllClones;
    AllClones.reserve(Sequences.size());
    for (const auto &C : Sequences) {
      AllClones.push_back(C);
    }

    Result.push_back(AllClones);

    constrainClones(Result, ConstraintList...);
  }

private:
  CloneGroup Sequences;
};

/// \brief Removes certain kind of clones from a list of clones.
///
/// This class should be the base class of all constraints that are used in
/// combination with the CloneDetector class.
/// As constraints are specified in the form of template parameters, this class
/// only contains non-virtual utility function to build custom constraints.
///
/// All subclasses are expected to have a method with the signature
///     `void constrain(std::vector<CloneDetector::CloneGroup> &Sequences)`
/// as this is the interface that the CloneDetector uses for applying the
/// constraint. The constraint is supposed to directly modify the passed list
/// so that all clones in the list fulfill the specific property this constraint
/// ensures.
class CloneConstraint {
protected:
  /// \brief Removes all groups by using a filter function.
  /// \param CloneGroups The list of CloneGroups that is supposed to be
  ///                    filtered.
  /// \param Filter The filter function that should return true for all groups
  ///               that should be removed from the list.
  void
  filterGroups(std::vector<CloneDetector::CloneGroup> &CloneGroups,
               std::function<bool(const CloneDetector::CloneGroup &)> Filter) {
    CloneGroups.erase(
        std::remove_if(CloneGroups.begin(), CloneGroups.end(), Filter),
        CloneGroups.end());
  }

  /// \brief Splits the given CloneGroups until the given Compare function
  ///        returns true for all clones in a single group.
  /// \param CloneGroups A list of CloneGroups that is modified in place.
  /// \param Compare The comparison function that all clones are supposed to
  ///                pass. Should return true if and only if two clones belong
  ///                to the same CloneGroup.
  void filterClones(
      std::vector<CloneDetector::CloneGroup> &CloneGroups,
      std::function<bool(const StmtSequence &, const StmtSequence &)> Compare);
};

/// \brief Splits group
class HashConstraint {

  size_t saveHash(const Stmt *S, ASTContext &Context,
                  std::vector<std::pair<size_t, StmtSequence>> &StmtsByHash);

public:
  void constrain(std::vector<CloneDetector::CloneGroup> &Sequences);
};

/// \brief Ensures that every clone has at least the given complexity.
///
/// Complexity is here defined as the total amount of children of a statement.
/// This constraint assumes the first statement in the group is representative
/// for all other statements in the group in terms of complexity.
class MinComplexityConstraint : public CloneConstraint {
  unsigned MinComplexity;

public:
  MinComplexityConstraint(unsigned MinComplexity)
      : MinComplexity(MinComplexity) {}

  size_t calculateStmtComplexity(const StmtSequence &Seq,
                                 const std::string &ParentMacroStack = "");

  void constrain(std::vector<CloneDetector::CloneGroup> &CloneGroups) {
    filterGroups(CloneGroups, [this](const CloneDetector::CloneGroup &A) {
      if (!A.empty())
        return calculateStmtComplexity(A.front()) < MinComplexity;
      else
        return false;
    });
  }
};

/// \brief Ensures that all clone groups contain at least the given amount of
///        clones.
class MinGroupSizeConstraint : CloneConstraint {
  unsigned MinGroupSize;

public:
  MinGroupSizeConstraint(unsigned MinGroupSize = 2)
      : MinGroupSize(MinGroupSize) {}

  void constrain(std::vector<CloneDetector::CloneGroup> &CloneGroups) {
    filterGroups(CloneGroups, [this](const CloneDetector::CloneGroup &A) {
      return A.size() < MinGroupSize;
    });
  }
};

/// \brief Ensures that no clone group fully contains another clone group.
///
/// This constraint searches for clones
struct OnlyLargestCloneConstraint : CloneConstraint {
  void constrain(std::vector<CloneDetector::CloneGroup> &Result);
};

/// \brief Analyzes the pattern of the referenced variables in a statement.
class VariablePattern {

  /// \brief Describes an occurence of a variable reference in a statement.
  struct VariableOccurence {
    /// The index of the associated VarDecl in the Variables vector.
    size_t KindID;
    /// The location in the code where the variable was references.
    SourceRange Location;

    VariableOccurence(size_t KindID, SourceRange Location)
        : KindID(KindID), Location(Location) {}
  };

  /// All occurences of referenced variables in the order of appearance.
  std::vector<VariableOccurence> Occurences;
  /// List of referenced variables in the order of appearance.
  /// Every item in this list is unique.
  std::vector<const VarDecl *> Variables;

  /// \brief Adds a new variable referenced to this pattern.
  /// \param VarDecl The declaration of the variable that is referenced.
  /// \param Location The SourceRange where this variable is referenced.
  void addVariableOccurence(const VarDecl *VarDecl, SourceRange Location);

  /// \brief Adds each referenced variable from the given statement.
  void addVariables(const Stmt *S);

public:
  /// \brief Creates an VariablePattern object with information about the given
  ///        StmtSequence.
  VariablePattern(const StmtSequence &Sequence) {
    for (const Stmt *S : Sequence)
      addVariables(S);
  }
  VariablePattern() {}

  /// \brief Describes two clones that reference their variables in a different
  ///        pattern which could indicate a programming error.
  struct SuspiciousClonePair {
    /// \brief Utility class holding the relevant information about a single
    ///        clone in this pair.
    struct SuspiciousCloneInfo {
      /// The variable which referencing in this clone was against the pattern.
      const VarDecl *Variable;
      /// Where the variable was referenced.
      SourceRange VarRange;
      /// The variable that should have been referenced to follow the pattern.
      /// If Suggestion is a nullptr then it's not possible to fix the pattern
      /// by referencing a different variable in this clone.
      const VarDecl *Suggestion;
      SuspiciousCloneInfo(const VarDecl *Variable, SourceRange Range,
                          const VarDecl *Suggestion)
          : Variable(Variable), VarRange(Range), Suggestion(Suggestion) {}
      SuspiciousCloneInfo() {}
    };
    /// The first clone in the pair which always has a suggested variable.
    SuspiciousCloneInfo FirstCloneInfo;
    /// This other clone in the pair which can have a suggested variable.
    SuspiciousCloneInfo SecondCloneInfo;
  };

  /// \brief Compares this pattern with the given one.
  /// \param Other The given VariablePattern to compare with.
  /// \param FirstMismatch Output parameter that will be filled with information
  ///        about the first difference between the two patterns. This parameter
  ///        can be a nullptr, in which case it will be ignored.
  /// \return Returns the number of differences between the pattern this object
  ///         is following and the given VariablePattern.
  ///
  /// For example, the following statements all have the same pattern and this
  /// function would return zero:
  ///
  ///   if (a < b) return a; return b;
  ///   if (x < y) return x; return y;
  ///   if (u2 < u1) return u2; return u1;
  ///
  /// But the following statement has a different pattern (note the changed
  /// variables in the return statements) and would have two differences when
  /// compared with one of the statements above.
  ///
  ///   if (a < b) return b; return a;
  ///
  /// This function should only be called if the related statements of the given
  /// pattern and the statements of this objects are clones of each other.
  unsigned getPatternDifferences(const VariablePattern &Other,
                                 SuspiciousClonePair *FirstMismatch = nullptr);
};

/// \brief Ensures that all clones reference variables in the same pattern.
struct MatchingVariablePatternConstraint : CloneConstraint {
  void constrain(std::vector<CloneDetector::CloneGroup> &CloneGroups);
};

} // end namespace clang

#endif // LLVM_CLANG_AST_CLONEDETECTION_H
