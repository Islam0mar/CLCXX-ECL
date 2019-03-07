#pragma once

#include "type_conversion.hpp"
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// Context
#include "clang/AST/ASTContext.h"

// Matchers
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

//
#include <vector>
#include <string>

namespace clcxx
{

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

class data {
  std::vector<string> parents;
  std::vector<string> template_names;
  std::vector<string> slots;
  std::vector<string> values;
  std::vector<string> methods_list;

  void clear(){
    parents.clear();
    template_names.clear();
    slots.clear();
    values.clear();
    methods_list.clear();
  }
};

data my_class;

class printer : public MatchFinder::MatchCallback {
 public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const CXXRecordDecl *FS =
            Result.Nodes.getNodeAs<clang::CXXRecordDecl>("a")) {
      for (auto p : FS->bases()) {
        my_class.parents.push_back(p.getType().getAsString());
        //std::cout << "base:" << p.getType().getAsString() << std::endl;
      }
      for (auto i : FS->fields()) {
        my_class.parents.push_back(p.getType().getAsString());
        // std::cout << "field: "<< FS->isDependentType() << i->getType().getAsString() << " "
        //           << i->getNameAsString() << std::endl;
      }
      for (auto i : FS->methods()) {
        std::cout << "method: "<<FS->isDependentType() << i->getType().getAsString() << " "
                  << i->getNameAsString() << std::endl;
      }
    }
  }
};


int searchAST(ClangTool Tool, const std::string &s) {
  auto m = recordDecl(isClass(), hasName(s), isDefinition()).bind("a");
  printer Printer;
  MatchFinder Finder;
  Finder.addMatcher(m, &Printer);
  return Tool.run(newFrontendActionFactory(&Finder).get());
}

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

int (void) {
  const char *y[] = {"sososhit", "header.cpp"};
  int x= 2;
  CommonOptionsParser OptionsParser(x, y, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  std::string s = "";
  while (s != "q") {
    std::getline(std::cin, s);
    searchAST(Tool, s);
  }
  // printer Printer;
  // MatchFinder Finder;
  // Finder.addMatcher(m, &Printer);
  // return Tool.run(newFrontendActionFactory(&Finder).get());
}



CLCXX_API cl_object ecl_defclass(cl_object name, cl_object package,
                                 cl_object super, cl_object options,
                                 std::queue<cl_object> slots,
                                 std::queue<cl_object> types);

} // namespace clcxx

