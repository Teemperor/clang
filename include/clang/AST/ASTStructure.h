//===--- AstStructure.h - Analyses the structure of an AST -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTStructure class, which can analyse the structure
//  of an AST.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_STRUCTURALHASH_H
#define LLVM_CLANG_AST_STRUCTURALHASH_H

#include <unordered_map>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <vector>

namespace clang {

///
/// \brief The Feature class describes a piece of code that is important
/// for the meaning of
///
class Feature {
  std::size_t NameIndex;
  SourceLocation StartLocation;
  SourceLocation EndLocation;
public:
  Feature() {
  }

  Feature(std::size_t NameIndex,
          SourceLocation StartLoc, SourceLocation EndLoc)
    : NameIndex(NameIndex), StartLocation(StartLoc), EndLocation(EndLoc) {
  }

  std::size_t getNameIndex() const {
    return NameIndex;
  }

  SourceLocation getStartLocation() const {
    return StartLocation;
  }

  SourceLocation getEndLocation() const {
    return EndLocation;
  }

  SourceRange getRange() const {
    return SourceRange(getStartLocation(), getEndLocation());
  }

  bool operator==(const Feature& Other) const {
    return NameIndex == Other.NameIndex && getRange() == Other.getRange();
  }

  bool operator!=(const Feature& Other) const {
    return NameIndex != Other.NameIndex && getRange() != Other.getRange();
  }
};

///
/// \brief A vector of features of a piece of code.
///
class FeatureVector {

  std::vector<Feature> Occurences;
  std::vector<std::string> FeatureNames;
  std::vector<QualType> FeatureTypes;

public:

  ///
  /// \brief Adds a new feature to the end of this vector.
  /// \param FeatureName The name of the feature.
  /// \param StartLocation The location in the source code where
  ///                      this feature starts.
  /// \param EndLocation The location in the source code where this
  ///                    feature starts.
  ///
  ///
  void add(const std::string& FeatureName, QualType FeatureType,
           SourceLocation StartLocation, SourceLocation EndLocation);

  const std::string& GetName(std::size_t FeatureId) const {
    assert(GetNumberOfNames() > FeatureId);
    return FeatureNames[FeatureId];
  }

  QualType GetType(std::size_t FeatureId) const {
    assert(GetNumberOfNames() > FeatureId);
    return FeatureTypes[FeatureId];
  }

  bool HasNameForIndex(std::size_t NameIndex) const {
    return GetNumberOfNames() > NameIndex;
  }

  Feature GetFeature(unsigned Index) const {
    return Occurences[Index];
  }

  std::size_t GetNumberOfNames() const {
    return FeatureNames.size();
  }

  bool operator==(const FeatureVector& Other) const {
    return Occurences == Other.Occurences && FeatureNames == Other.FeatureNames;
  }

  bool operator!=(const FeatureVector& Other) const {
    return Occurences != Other.Occurences && FeatureNames != Other.FeatureNames;
  }

  struct ComparisonResult {
    unsigned MismatchingFeatureIndex;
    bool Success;
    bool Incompatible;
    unsigned TotalErrorNumber;
  public:
    ComparisonResult() : Success(true), Incompatible(false) {
    }

  };

  ComparisonResult compare(const FeatureVector& other) {
    ComparisonResult result;
    unsigned FirstErrorIndex;
    unsigned TotalErrorNumber = 0;
    if (Occurences.size() != other.Occurences.size()) {
      result.Incompatible = true;
      result.Success = false;
      return result;
    }
    for (unsigned I = 0; I < Occurences.size(); ++I) {
      if (Occurences[I].getNameIndex() != other.Occurences[I].getNameIndex()) {
        if (TotalErrorNumber == 0) {
          FirstErrorIndex = I;
        }
        ++TotalErrorNumber;
      }
    }
    if (TotalErrorNumber != 0) {
      result.Success = false;
      result.Incompatible = false;
      result.MismatchingFeatureIndex = FirstErrorIndex;
      result.TotalErrorNumber = TotalErrorNumber;
      return result;
    }

    result.Success = true;
    result.Incompatible = false;
    return result;
  }

};

///
/// \brief Stores a piece of (executable) code. It can either hold
/// a single Stmt or a sequence of statements inside a CompoundStmt.
///
class StmtSequence {
  Stmt *S;
  ASTContext* Context;
  // If EndIndex is non-zero, then S is a CompoundStmt and this StmtSequence
  // instance is representing the children inside the array [Start, End);
  unsigned StartIndex;
  unsigned EndIndex;
  bool Macro = false;
public:
  StmtSequence(Stmt *Stmt, ASTContext* Context, unsigned StartIndex, unsigned EndIndex);

  StmtSequence(Stmt *Stmt, ASTContext* Context) : StmtSequence(Stmt, Context, 0, 0) {
  }
  StmtSequence() : StmtSequence(nullptr, nullptr, 0, 0) {
  }

  Stmt* GetStmt() const {
    return S;
  }

  ASTContext& GetASTContext() {
    return *Context;
  }

  unsigned GetStartIndex() const {
    return StartIndex;
  }

  unsigned GetEndIndex() const {
    return EndIndex;
  }

  bool IsMacro() const {
    return Macro;
  }

  bool HoldsSequence() const {
    return EndIndex != 0;
  }

  SourceLocation getLocStart() const {
    if (HoldsSequence()) {
      auto CS = static_cast<CompoundStmt*>(S);
      return CS->body_begin()[StartIndex]->getLocStart();
    }
    return S->getLocStart();
  }

  SourceLocation getLocEnd() const {
    if (HoldsSequence()) {
      auto CS = static_cast<CompoundStmt*>(S);
      return CS->body_begin()[EndIndex - 1]->getLocEnd();
    }
    return S->getLocEnd();
  }

  bool operator==(const StmtSequence& other) const {
    return S == other.S &&
        StartIndex == other.StartIndex &&
        EndIndex == other.EndIndex;
  }

  bool contains(const StmtSequence& other) const;

  bool equal(const StmtSequence& other);
};

}

namespace std {
  template <>
  struct hash<clang::StmtSequence>
  {
    size_t operator()(const clang::StmtSequence &Info) const
    {
      return ((std::hash<clang::Stmt *>()(Info.GetStmt())
               ^ (std::hash<unsigned>()(Info.GetStartIndex()) << 1)) >> 1)
               ^ (std::hash<unsigned>()(Info.GetEndIndex()) << 1);
    }
  };
}

namespace clang {

class StmtFeature {

public:
  enum StmtFeatureKind {
    VariableName = 0,
    FunctionName = 1,
    END
  };

  StmtFeature(StmtSequence S);

  void add(const std::string& Name, SourceLocation StartLoc,
           SourceLocation EndLoc, StmtFeatureKind Kind,
           QualType T);

  struct CompareResult {
    StmtFeatureKind MismatchKind;
    FeatureVector::ComparisonResult result;
    FeatureVector FeaturesThis;
    FeatureVector FeaturesOther;
    CompareResult(StmtFeatureKind MismatchKind,
                  FeatureVector::ComparisonResult result,
                  FeatureVector FeaturesThis,
                  FeatureVector FeaturesOther)
      : MismatchKind(MismatchKind), result(result), FeaturesThis(FeaturesThis),
        FeaturesOther(FeaturesOther){
    }

    CompareResult()
      : MismatchKind(StmtFeatureKind::END) {
    }
  };

  FeatureVector& GetFeatureVector(StmtFeatureKind Kind) {
    return Features[Kind];
  }

  unsigned differentFeatureVectors(const StmtFeature& other);

  CompareResult compare(const StmtFeature& other) {
    for (unsigned Kind = 0; Kind < END; ++Kind) {
      FeatureVector::ComparisonResult vectorResult =
          Features[Kind].compare(other.Features[Kind]);
      if (!vectorResult.Incompatible && !vectorResult.Success) {
        return CompareResult(static_cast<StmtFeatureKind>(Kind), vectorResult,
                             Features[Kind], other.Features[Kind]);
      }
    }
    return CompareResult();
  }

private:
  FeatureVector Features[END];
};


/// ASTStructure - This class analyses the structure of the Stmts
/// in a given AST and is intended to be used to find sub-trees with identical
/// structure. The structure of a tree equals the functionality of the
/// code behind it.
///
/// Specifically this class provides a locality-sensitive hash function
/// for Stmts that generates colliding hash values for nodes with the same
/// structure.
///
/// This is done by only hashing information that describes structure
/// (e.g. the type of each node).
///
/// Other information such as the names of variables, classes
/// and other parts of the program are ignored.
class ASTStructure {

public:

  struct StmtData {
    unsigned Hash;
    unsigned Complexity;
    StmtData() {
    }

    StmtData(unsigned Hash, unsigned Complexity)
      : Hash(Hash), Complexity(Complexity) {
    }
  };

  /// \brief ASTStructure generates information about the Stmts in the AST.
  ///
  /// \param Context The AST that shall be analyzed.
  ///
  /// Analyses the Stmts in the given AST and stores all information
  /// about their structure in the newly created ASTStructure object.
  explicit ASTStructure(ASTContext &Context);

  struct HashSearchResult {
    StmtData Data;
    bool Success;
  };

  /// \brief Looks up the structure hash code for the given Stmt
  ///        in the storage of this ASTStructure object.
  ///
  /// \param S the given Stmt.
  ///
  /// \returns A \c HashSearchResult containing information of whether the
  ///          search was successful and if yes the found hash code.
  ///
  /// The structure hash code is a integer describing the structure
  /// of the given Stmt. Stmts with an equal structure hash code probably
  /// have the same structure.
  HashSearchResult findHash(StmtSequence S) {
    auto I = HashedStmts.find(S);
    if (I == HashedStmts.end()) {
      return {StmtData(), false};
    }
    return {I->second, true};
  }

  /// \brief Adds a Stmt with the associated structure hash code to the storage
  ///        of this ASTStructure object.
  ///
  /// \param Hash the hash code of the given statement.
  /// \param Complexity the Complexity of the given statement.
  /// \param S the given statement.
  void add(unsigned Hash, unsigned Complexity, StmtSequence S) {
    HashedStmts.insert(std::make_pair(S,
                                      StmtData(Hash, Complexity)));
  }

  class CloneMismatchPart {
    StmtSequence Stmt;
    FeatureVector Features;
    std::string SuggestedFeature;
    unsigned MismatchIndex;
  public:
    CloneMismatchPart(StmtSequence Stmt, FeatureVector Features,
                      unsigned MismatchIndex)
      : Stmt(Stmt), Features(Features), MismatchIndex(MismatchIndex) {
    }
    CloneMismatchPart() {
    }

    void SuggestFeature(const std::string& Feature) {
      SuggestedFeature = Feature;
    }

    StmtSequence GetStmt() const {
      return Stmt;
    }

    Feature GetFeature() const {
      return Features.GetFeature(MismatchIndex);
    }

    std::string GetFeatureName() const {
      return Features.GetName(GetFeature().getNameIndex());
    }

    bool HasSuggestion() {
      return !SuggestedFeature.empty();
    }

    const std::string& GetSuggestion() const {
      return SuggestedFeature;
    }

    const FeatureVector& GetFeatures() const {
      return Features;
    }
  };

  struct CloneMismatch {
    CloneMismatchPart A;
    CloneMismatchPart B;
    unsigned MismatchIndex;
    StmtFeature::StmtFeatureKind MismatchKind;
  public:
    CloneMismatch() {
    }

    CloneMismatch(CloneMismatchPart OnePart, CloneMismatchPart OtherPart,
                  unsigned MismatchIndex,
                  StmtFeature::StmtFeatureKind MismatchKind);
  };

  typedef std::vector<StmtSequence> CloneGroup;

  std::vector<CloneMismatch> FindCloneErrors(unsigned MinGroupComplexity = 50);

  std::vector<CloneGroup> FindClones(unsigned MinGroupComplexity = 50);

private:
  std::unordered_map<StmtSequence, StmtData> HashedStmts;

};

} // end namespace clang

#endif
