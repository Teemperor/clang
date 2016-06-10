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
#include <vector>

namespace clang {


class Feature {
  std::size_t NameIndex;
  SourceLocation Location;
public:
  Feature() {
  }

  Feature(std::size_t NameIndex, SourceLocation Loc)
    : NameIndex(NameIndex), Location(Loc) {
  }

  std::size_t getNameIndex() const {
    return NameIndex;
  }

  SourceLocation getLocation() const {
    return Location;
  }
};

class FeatureVector {

  std::vector<Feature> Locations;
  std::vector<std::string> FeatureNames;
public:

  void add(const std::string& FeatureName, SourceLocation Location) {
    for (std::size_t I = 0; I < FeatureNames.size(); ++I) {
      if (FeatureNames[I] == FeatureName) {
          Locations.push_back({I, Location});
          return;
      }
    }
    Locations.push_back({Locations.size(), Location});
    FeatureNames.push_back(FeatureName);
  }

  const std::string& getName(std::size_t NameIndex) {
    return FeatureNames[NameIndex];
  }

  struct ComparisonResult {
    Feature FeatureThis;
    Feature FeatureOther;
    bool Success;
    bool Incompatible;
  };

  ComparisonResult compare(const FeatureVector& other) {
    ComparisonResult result;
    if (Locations.size() != other.Locations.size()) {
      result.Incompatible = true;
      result.Success = false;
      return result;
    }
    for (std::size_t I = 0; I < Locations.size(); ++I) {
      if (Locations[I].getNameIndex() != other.Locations[I].getNameIndex()) {
        result.Success = false;
        result.Incompatible = false;
        result.FeatureThis = Locations[I];
        result.FeatureOther = other.Locations[I];
        return result;
      }
    }

    result.Success = true;
    result.Incompatible = false;
    return result;
  }

};

class StmtFeature {

public:
  enum StmtFeatureKind {
    Variable = 0,
    Function,
    Literal,
    END
  };

  StmtFeature(Stmt *S);

  struct CompareResult {
    StmtFeatureKind MismatchKind;
    FeatureVector::ComparisonResult result;
  };

  CompareResult compare(const StmtFeature& other) {
    for (unsigned Kind = 0; Kind < END; ++Kind) {
      FeatureVector::ComparisonResult vectorResult =
          Features[Kind].compare(other.Features[Kind]);
      if (!vectorResult.Success) {
        return {static_cast<StmtFeatureKind>(Kind), vectorResult};
      }
    }
    return {END, FeatureVector::ComparisonResult()};
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
/// for Stmts that generates colliding hash values
/// for nodes with the same structure.
///
/// This is done by only hashing
/// information that describes structure (e.g. the type of each
/// node).
/// Other information such as the names of variables, classes
/// and other parts of the program are ignored.
class ASTStructure {

  std::unordered_map<Stmt *, unsigned> HashedStmts;

public:
  ///
  /// \brief ASTStructure generates information about the Stmts in the AST.
  ///
  /// \param Context The AST that shall be analyzed.
  ///
  /// Analyses the Stmts in the given AST and stores all information
  /// about their structure in the newly created ASTStructure object.
  explicit ASTStructure(ASTContext &Context);

  struct HashSearchResult {
    unsigned Hash;
    bool Success;
  };

  ///
  /// \brief Looks up the structure hash code for the given Stmt
  ///        in the storage of this ASTStructure object.
  ///
  /// \param S the given Stmt.
  ///
  /// \returns A \c HashSearchResult containing information of whether
  ///          the search was successful and if yes the found hash code.
  ///
  /// The structure hash code is a integer describing the structure
  /// of the given Stmt. Stmts with an equal structure hash code probably
  /// have the same structure.
  HashSearchResult findHash(Stmt *S) {
    auto I = HashedStmts.find(S);
    if (I == HashedStmts.end()) {
      return {0, false};
    }
    return {I->second, true};
  }
  ///
  /// \brief Adds with the given Stmt with the associated structure hash code
  ///        to the storage of this ASTStructure object.
  ///
  /// \param Hash the hash code of the given Stmt.
  /// \param S the given Stmt.
  void add(unsigned Hash, Stmt *S) {
    HashedStmts.insert(std::make_pair(S, Hash));
  }

  std::vector<StmtFeature::CompareResult> findCloneErrors();
};

} // end namespace clang

#endif
