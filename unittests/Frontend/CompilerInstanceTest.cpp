//===- unittests/Frontend/CompilerInstanceTest.cpp - ASTUnit tests --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <fstream>

#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/PCHContainerOperations.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ToolOutputFile.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace clang;

namespace {

TEST(CompilerInstance, VFSOverlay) {
  llvm::SmallString<256> CurrentPath;
  llvm::sys::fs::current_path(CurrentPath);

  int VFSFD;
  llvm::SmallString<256> VFSFileName;
  ASSERT_FALSE(
      llvm::sys::fs::createTemporaryFile("vfs", "yaml", VFSFD, VFSFileName));
  tool_output_file VFSFile(VFSFileName, VFSFD);

  llvm::sys::fs::make_absolute(CurrentPath, VFSFileName);

  VFSFile.os() << "{ 'version': 0, 'roots': [\n"
                  "  { 'name': '"
               << CurrentPath
               << "',\n"
                  "    'type': 'directory',\n"
                  "    'contents': [\n"
                  "      { 'name': 'virtual.file', 'type': 'file',\n"
                  "        'external-contents': '"
               << VFSFileName << "'\n"
                                 "      }\n"
                                 "    ]\n"
                                 "  }\n"
                                 "]}\n";
  VFSFile.os().flush();

  int FD;
  llvm::SmallString<256> InputFileName;
  ASSERT_FALSE(
      llvm::sys::fs::createTemporaryFile("vfs", "cpp", FD, InputFileName));
  tool_output_file input_file(InputFileName, FD);
  input_file.os() << "";

  std::string VFSArg = "-ivfsoverlay" + VFSFileName.str().str();

  const char *Args[] = {"clang", VFSArg.c_str(), "-xc++",
                        InputFileName.c_str()};

  IntrusiveRefCntPtr<DiagnosticsEngine> Diags =
      CompilerInstance::createDiagnostics(new DiagnosticOptions());

  std::shared_ptr<CompilerInvocation> CInvok =
      createInvocationFromCommandLine(Args, Diags);

  if (!CInvok)
    FAIL() << "could not create compiler invocation";
  CompilerInstance Instance;
  Instance.setDiagnostics(Diags.get());
  Instance.setInvocation(CInvok);
  Instance.createFileManager();

  ASSERT_TRUE(Instance.getFileManager().getFile("virtual.file"));
  ASSERT_FALSE(Instance.getFileManager().getFile("virtual.file2"));
}

} // anonymous namespace
