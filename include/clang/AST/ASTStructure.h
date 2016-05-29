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

/// ASTStructure - This class analyses the structure of the nodes
/// in a given AST and is intended to be used to find sub-trees with identical
/// structure. The structure of a tree equals the functionality of the
/// code behind it.
///
/// Specifically this class provides a locality-sensitive hash function
/// for AST nodes that generates colliding hash values
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
  /// \brief ASTStructure Analyses the given AST
  ///
  /// \param Context The AST that shall be analyzed.
  ///
  /// Analyses the given AST and stores all information
  /// about its structure in the newly created ASTStructure object.
  explicit ASTStructure(ASTContext& Context);

  struct HashSearchResult {
    unsigned Hash;
    bool Success;
  };

  HashSearchResult findHash(const Stmt *S) {
    auto I = HashedStmts.find(S);
    if (I == HashedStmts.end()) {
      return {0, false};
    }
    return {I->second, true};
  }

  unsigned getHash(const Stmt *S) {
    auto I = HashedStmts.find(S);
    if (I == HashedStmts.end()) {
      assert("getHash(const Stmt *D) called on unknown Stmt");
    }
    return I->second;
  }

  void add(unsigned Hash, const Stmt *Stmt) {
    HashedStmts.insert(std::make_pair(Stmt, Hash));
  }
};

} // end namespace clang

#endif
