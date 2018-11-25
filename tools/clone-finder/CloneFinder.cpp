//===--- tools/clang-check/ClangCheck.cpp - Clang check tool --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements a clang-check tool that runs clang based on the info
//  stored in a compilation database.
//
//  This tool uses the Clang Tooling infrastructure, see
//    http://clang.llvm.org/docs/HowToSetupToolingForLLVM.html
//  for details on setting it up with LLVM source tree.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Rewrite/Frontend/FrontendActions.h"
#include "clang/StaticAnalyzer/Frontend/FrontendActions.h"
#include "clang/Analysis/CloneDetection.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <set>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

void printLines(std::string str, unsigned start, unsigned stop) {
  std::stringstream ss(str);
  std::string line;
  unsigned index = 0;
  while(std::getline(ss, line, '\n')){
    index++;
    if (index >= start && index <= stop)
      std::cout << line << '\n';
  }
}

struct HashWithID {
  size_t Hash;
  size_t ID;
  bool operator<(const HashWithID &O) const {
    return std::tie(Hash, ID) < std::tie(O.Hash, ID);
  }
};

struct ParseJob {
  CompileCommand CC;
  size_t FileIndex;
};

static std::unordered_set<std::string> HandledFileSets;

static bool ShouldCheckSet(std::set<size_t> FileIndexSet) {
  std::string Value = "";
  for (size_t i : FileIndexSet) {
    Value.append(std::to_string(i));
    Value.push_back(';');
  }
  auto It = HandledFileSets.find(Value);
  if (It == HandledFileSets.end()) {
    HandledFileSets.insert(Value);
    return true;
  }
  return false;
}

static unsigned MinComplexity = 100;

struct ImportantDeclVisitor : RecursiveASTVisitor<ImportantDeclVisitor> {
  std::unordered_set<std::string> Decls;
  ParseJob Job;
  std::vector<HashWithID>& AllHashes;
  ImportantDeclVisitor(std::vector<HashWithID>& Hashes) : AllHashes(Hashes) {
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    if (!D->hasBody())
      return true;
    if (!D->getASTContext().getSourceManager().isInMainFile(D->getLocation()))
      return true;
    std::string Name = D->getQualifiedNameAsString();
    if (Decls.count(Name) != 0)
      return true;
    Decls.insert(Name);
    RecursiveCloneTypeIIHashConstraint C;
    std::vector<std::pair<size_t, StmtSequence>> NewHashes;
    C.saveHash(D->getBody(), D, NewHashes);
    MinComplexityConstraint MinC(MinComplexity);

    for (auto &Hash : NewHashes) {
      size_t Compl = MinC.calculateStmtComplexity(Hash.second, MinComplexity);
      if (Compl >= MinComplexity) {
        AllHashes.push_back({Hash.first, Job.FileIndex});
      }
    }
    return true;
  }
};

struct DeclVisitor2 : RecursiveASTVisitor<DeclVisitor2> {
  std::unordered_set<std::string> Decls;
  CloneDetector &Detector;
  DeclVisitor2(CloneDetector &D) : Detector(D) {}

  bool VisitFunctionDecl(FunctionDecl *D) {
    if (!D->hasBody())
      return true;
    if (!D->getASTContext().getSourceManager().isInMainFile(D->getLocation()))
      return true;
    std::string Name = D->getQualifiedNameAsString();
    if (Decls.count(Name) != 0)
      return true;
    Decls.insert(Name);

    Detector.analyzeCodeBody(D);

    return true;
  }
};

struct SourceFile {
  CompileCommand CC;
};
std::vector<SourceFile> FileList;

std::string getSourceCode(const std::string &Path) {
  std::ifstream codeFile(Path);
  codeFile.seekg(0, std::ios::end);
  auto size = codeFile.tellg();
  std::string Code(static_cast<size_t>(size), '\0');
  codeFile.seekg(0);
  codeFile.read(&Code[0], size);
  return Code;
}

static void scanFileSet(std::set<size_t> FileIndexSet) {
  std::vector<clang::CloneDetector::CloneGroup> CloneGroups;

  clang::CloneDetector Detector;

  DeclVisitor2 Visitor(Detector);

  std::list<std::unique_ptr<ASTUnit> > Units;
  for (size_t ID : FileIndexSet) {
    auto CC = FileList.at(ID).CC;
    std::string *Code = new std::string(getSourceCode(CC.Filename));

    std::unique_ptr<ASTUnit> ASTUnit =
        clang::tooling::buildASTFromCodeWithArgs(*Code, CC.CommandLine, CC.Filename, "clone-finder");
    Visitor.TraverseTranslationUnitDecl(ASTUnit->getASTContext().getTranslationUnitDecl());
    Units.push_back(std::move(ASTUnit));
  }

  Detector.findClones(
      CloneGroups, RecursiveCloneTypeIIHashConstraint(),
      MinGroupSizeConstraint(2), MinComplexityConstraint(MinComplexity),
      //RecursiveCloneTypeIIVerifyConstraint(),
      NoOverlappingCloneConstraint(),
      OnlyLargestCloneConstraint());

  std::cout << "Found " << CloneGroups.size() << " clones" << std::endl;

  for (auto&CloneGroup : CloneGroups) {
    std::cout << "GROUP:" << "\n";
    for (auto &Clone : CloneGroup) {
      auto &SM = Clone.getASTContext().getSourceManager();
      unsigned StartLine = SM.getSpellingLineNumber(Clone.front()->getBeginLoc());
      unsigned EndLine = SM.getSpellingLineNumber(Clone.back()->getEndLoc());
      std::string File = SM.getFilename(Clone.front()->getBeginLoc()).str();
      std::cout << "File: " << File;
      std::cout << ":" << StartLine;
      std::cout << "->"<< EndLine;
      std::cout << "\n";
      printLines(getSourceCode(File), StartLine, EndLine);
     // printLines(*ASTToCode[&Clone.getASTContext()], StartLine, EndLine);
    }
  }
}

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Initialize targets for clang module support.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  std::string error;
  std::unique_ptr<CompilationDatabase> DB = JSONCompilationDatabase::loadFromDirectory(".", error);

  if (!DB && !error.empty()) {
    std::cerr << error << std::endl;
    return 1;
  }

  unsigned done = 0;
  size_t limit = 7000;
  const unsigned NumberOfThreads = 6;
  std::mutex lock;

  std::vector<std::thread *> Threads;

  std::list<ParseJob> Jobs;

  for (CompileCommand CC1 : DB->getAllCompileCommands()) {
      done++;
      if (done > limit)
        break;

      CC1.CommandLine.pop_back();

      auto &v = CC1.CommandLine;
      auto itr = std::find(v.begin(), v.end(), "-Wunused-command-line-argument");
      if (itr != v.end()) v.erase(itr);


      CC1.CommandLine.push_back("-Wno-unused-command-line-argument");
      CC1.CommandLine.push_back("-I");
      CC1.CommandLine.push_back("/home/teemperor/llvm/rel-build/lib/clang/8.0.0/include/");
      CC1.CommandLine.push_back("-resource-dir");
      CC1.CommandLine.push_back("/home/teemperor/llvm/rel-build/lib/clang/8.0.0/");

      Jobs.push_back({CC1, FileList.size()});
      FileList.push_back({CC1});
  }
  limit = std::min(Jobs.size(), limit);

  std::vector<HashWithID> AllHashes;
  ImportantDeclVisitor Visitor(AllHashes);

  for (unsigned i = 1; i <= NumberOfThreads; i++) {
      auto t = new std::thread([i, &Jobs, &lock, &Visitor, limit]() {
          while (true) {
            ParseJob Job;
            {
              std::lock_guard<std::mutex> guard(lock);
              if (Jobs.empty())
                break;

              Job = Jobs.front();
              Jobs.pop_front();
              std::cout << "T" << i << " [" << (limit - Jobs.size()) << "/" << limit << "] " << Job.CC.Filename << std::endl;
            }

            CompileCommand CC = Job.CC;

            std::string Code = getSourceCode(CC.Filename);

            std::unique_ptr<ASTUnit> ASTUnit =
                clang::tooling::buildASTFromCodeWithArgs(Code, CC.CommandLine, CC.Filename, "clone-finder");
            {
              std::lock_guard<std::mutex> guard(lock);
              Visitor.Job = Job;
              Visitor.TraverseTranslationUnitDecl(ASTUnit->getASTContext().getTranslationUnitDecl());
            }
          }
      });
      Threads.push_back(t);
  }

  for (size_t i = 0; i < Threads.size(); ++i) {
    Threads[i]->join();
    delete Threads[i];
  }

  std::stable_sort(AllHashes.begin(), AllHashes.end(),
                   [](const HashWithID& A, const HashWithID &B){
    return A.Hash < B.Hash;
  });

  std::cout << "Scanning..." << std::endl;

  if (AllHashes.size() == 0) {
    std::cout << "No hashes found?" << std::endl;
    return 1;
  }

  for (unsigned i = 0; i < AllHashes.size() - 1; ++i) {
    const auto Current = AllHashes[i];

    // It's likely that we just found a sequence of StmtSequences that
    // represent a CloneGroup, so we create a new group and start checking and
    // adding the StmtSequences in this sequence.
    std::set<size_t> FileIndexSet;

    size_t PrototypeHash = Current.Hash;

    size_t StartI = i;
    for (; i < AllHashes.size(); ++i) {
      // A different hash value means we have reached the end of the sequence.
      if (PrototypeHash != AllHashes[i].Hash) {
        --i;
        break;
      }
      // Same hash value means we should add the StmtSequence to the current
      // group.
      FileIndexSet.insert(AllHashes[i].ID);
    }

    if (StartI != i) {
      if (ShouldCheckSet(FileIndexSet))
        scanFileSet(FileIndexSet);
    }
  }
}
