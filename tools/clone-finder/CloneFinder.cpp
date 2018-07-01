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
  while(std::getline(ss,line,'\n')){
    index++;
    if (index >= start && index <= stop)
      std::cout << line << '\n';
  }
}

struct ImportantDeclVisitor : RecursiveASTVisitor<ImportantDeclVisitor> {
  CloneDetector* Detector;
  std::unordered_set<std::string> Decls;
  bool VisitFunctionDecl(FunctionDecl *D) {
    if (!D->hasBody())
      return true;
    if (!D->getASTContext().getSourceManager().isInMainFile(D->getLocation()))
      return true;
    std::string Name = D->getNameAsString();
    if (Decls.count(Name) != 0)
      return true;
    Decls.insert(Name);
    Detector->analyzeCodeBody(D);
    return true;
  }
};

struct ParseJob {
  CompileCommand CC;
};

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

  clang::CloneDetector Detector;


  unsigned done = 0;
  unsigned limit = 100;
  std::vector<std::string *> SourceCode;
  std::map<ASTContext *, std::string *> ASTToCode;
  std::mutex lock;

  std::vector<std::thread *> Threads;

  std::list<ParseJob> Jobs;

  for (CompileCommand CC1 : DB->getAllCompileCommands()) {
      done++;
      if (done > limit)
        break;
      Jobs.push_back({CC1});
  }

  for (unsigned i = 1; i <= 7; i++) {
      auto t = new std::thread([i, &Jobs, &Detector, &SourceCode, &ASTToCode, &lock, limit]() {
          while (true) {

            ParseJob job;

            {
              std::lock_guard<std::mutex> guard(lock);
              if (Jobs.empty())
                break;

              job = Jobs.front();
              Jobs.pop_front();
              std::cout << "T" << i << " [" << (limit - Jobs.size()) << "/" << limit << "] " << job.CC.Filename << std::endl;
            }

            CompileCommand CC = job.CC;

            ImportantDeclVisitor Visitor;
            Visitor.Detector = &Detector;

            CC.CommandLine.pop_back();

            std::ifstream codeFile(CC.Filename);
            codeFile.seekg(0, std::ios::end);
            auto size = codeFile.tellg();
            std::string *Code = new std::string(size, '\0');
            codeFile.seekg(0);
            codeFile.read(&(*Code)[0], size);

            //CC.CommandLine.push_back("-I");
            //CC.CommandLine.push_back("/home/teemperor/clones/rel/lib/clang/5.0.0/include/");
            CC.CommandLine.push_back("-resource-dir");
            CC.CommandLine.push_back("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/9.1.0");
            CC.CommandLine.push_back("-isysroot");
            CC.CommandLine.push_back("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk");
            CC.CommandLine.push_back("-I/usr/local/include");
            CC.CommandLine.push_back("-stdlib=libc++");
            CC.CommandLine.push_back("-Wno-unused-command-line-argument");

            auto &v = CC.CommandLine;
            auto itr = std::find(v.begin(), v.end(), "-Wunused-command-line-argument");
            if (itr != v.end()) v.erase(itr);

            std::unique_ptr<ASTUnit> ASTUnit =
                clang::tooling::buildASTFromCodeWithArgs(*Code, CC.CommandLine, CC.Filename, "clone-finder");
            {
              std::lock_guard<std::mutex> guard(lock);
              SourceCode.push_back(Code);
              ASTToCode[&ASTUnit->getASTContext()] = Code;
              Visitor.TraverseTranslationUnitDecl(ASTUnit->getASTContext().getTranslationUnitDecl());
              ASTUnit.release();
            }
          }
      });
      Threads.push_back(t);
  }

  for (size_t i = 0; i < Threads.size(); ++i) {
    Threads[i]->join();
    delete Threads[i];
  }

  std::cout << "Scanning..." << std::endl;

  std::vector<CloneDetector::CloneGroup> AllCloneGroups;

  Detector.findClones(
      AllCloneGroups, RecursiveCloneTypeIIHashConstraint(),
      MinGroupSizeConstraint(2), MinComplexityConstraint(100),
      //RecursiveCloneTypeIIVerifyConstraint(),
        OnlyLargestCloneConstraint());

  std::cout << "Found " << AllCloneGroups.size() << " clones" << std::endl;

  for (auto&CloneGroup : AllCloneGroups) {
    std::cout << "GROUP:" << "\n";
    for (auto &Clone : CloneGroup) {
      unsigned StartLine = Clone.getASTContext().getSourceManager().getSpellingLineNumber(Clone.front()->getLocStart());
      unsigned EndLine = Clone.getASTContext().getSourceManager().getSpellingLineNumber(Clone.back()->getLocEnd());
      std::cout << "File: " << Clone.getASTContext().getSourceManager().getFilename(Clone.front()->getLocStart()).str();
      std::cout << ":" << StartLine;
      std::cout << "->"<< EndLine;
      std::cout << "\n";
     // printLines(*ASTToCode[&Clone.getASTContext()], StartLine, EndLine);
    }
  }

  // Now that the suspicious clone detector has checked for pattern errors,
  // we also filter all clones who don't have matching patterns
  //CloneDetector::constrainClones(AllCloneGroups,
  //                               MatchingVariablePatternConstraint(),
  //                               MinGroupSizeConstraint(2));

}
