#include <iostream>
#include <string>

#include "lorsc/Codegen.h"
#include "lorsc/Diagnostics.h"
#include "lorsc/Parser.h"
#include "lorsc/Sema.h"

namespace {

struct Options {
  std::string input;
  std::string outputObject;
  std::string outputIR;
  std::string outputAsm;
  std::string targetTriple = "riscv64-unknown-linux-gnu";
};

void printUsage() {
  std::cerr << "Usage: lorsc <input> -o <output.o> [--emit-llvm <output.ll>] [--emit-asm <output.s>]"
               " [--target <triple>]\n";
}

bool parseArgs(int argc, char** argv, Options& options) {
  if (argc < 4) {
    return false;
  }

  options.input = argv[1];
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) {
      options.outputObject = argv[++i];
    } else if (arg == "--emit-llvm" && i + 1 < argc) {
      options.outputIR = argv[++i];
    } else if (arg == "--emit-asm" && i + 1 < argc) {
      options.outputAsm = argv[++i];
    } else if (arg == "--target" && i + 1 < argc) {
      options.targetTriple = argv[++i];
    } else if (arg.rfind("--emit-asm=", 0) == 0) {
      options.outputAsm = arg.substr(std::string("--emit-asm=").size());
    } else if (arg.rfind("--target=", 0) == 0) {
      options.targetTriple = arg.substr(std::string("--target=").size());
    } else {
      std::cerr << "Unknown or incomplete argument: " << arg << "\n";
      return false;
    }
  }

  return !options.input.empty() && !options.outputObject.empty();
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parseArgs(argc, argv, options)) {
    printUsage();
    return 1;
  }

  lorsc::DiagnosticsEngine diagnostics;
  std::unique_ptr<lorsc::Program> program = lorsc::parseFile(options.input, diagnostics);
  if (!program) {
    diagnostics.printToStderr();
    return 1;
  }

  lorsc::SemanticAnalyzer sema(diagnostics, options.input);
  if (!sema.analyze(*program) || diagnostics.hasErrors()) {
    diagnostics.printToStderr();
    return 1;
  }

  lorsc::CodeGenerator codegen(options.targetTriple);
  if (!codegen.generate(*program)) {
    return 1;
  }

  if (!options.outputIR.empty() && !codegen.emitIR(options.outputIR)) {
    return 1;
  }
  if (!options.outputAsm.empty() && !codegen.emitAssembly(options.outputAsm)) {
    return 1;
  }
  if (!codegen.emitObject(options.outputObject)) {
    return 1;
  }

  return 0;
}
