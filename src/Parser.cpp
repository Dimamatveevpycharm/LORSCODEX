#include "lorsc/Parser.h"

#include <cstdio>
#include <memory>
#include <string>

namespace lorsc {
DiagnosticsEngine* gDiagnostics = nullptr;
std::string gInputFile;
Program* gParsedProgram = nullptr;
}  // namespace lorsc

extern int yyparse(void);
extern FILE* yyin;
extern int yylineno;
extern int yycolumn;

namespace lorsc {

std::unique_ptr<Program> parseFile(const std::string& filePath, DiagnosticsEngine& diagnostics) {
  FILE* in = std::fopen(filePath.c_str(), "rb");
  if (in == nullptr) {
    diagnostics.reportError(filePath, {1, 1}, "cannot open input file");
    return nullptr;
  }

  gDiagnostics = &diagnostics;
  gInputFile = filePath;
  gParsedProgram = nullptr;
  yyin = in;
  yylineno = 1;
  yycolumn = 1;

  int parseResult = yyparse();
  std::fclose(in);
  yyin = nullptr;

  if (parseResult != 0 || gParsedProgram == nullptr || diagnostics.hasErrors()) {
    gParsedProgram = nullptr;
    gDiagnostics = nullptr;
    gInputFile.clear();
    return nullptr;
  }

  std::unique_ptr<Program> program(gParsedProgram);
  gParsedProgram = nullptr;
  gDiagnostics = nullptr;
  gInputFile.clear();
  return program;
}

}  // namespace lorsc
