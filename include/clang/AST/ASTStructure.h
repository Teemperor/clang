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

namespace clang {

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

  std::unordered_map<const Stmt *, unsigned> HashedStmts;

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
  HashSearchResult findHash(const Stmt *S) {
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
  void add(unsigned Hash, const Stmt *S) {
    HashedStmts.insert(std::make_pair(S, Hash));
  }
};

} // end namespace clang

#endif
