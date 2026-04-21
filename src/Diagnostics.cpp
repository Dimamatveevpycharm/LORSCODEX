#include "lorsc/Diagnostics.h"

#include <iostream>

namespace lorsc {

void DiagnosticsEngine::reportError(const std::string& file, SourceLocation loc, const std::string& message) {
  errors_.push_back(Diagnostic{file, loc, message});
}

void DiagnosticsEngine::reportParseError(const std::string& file, int line, int column,
                                         const std::string& message) {
  errors_.push_back(Diagnostic{file, SourceLocation{line, column}, message});
}

void DiagnosticsEngine::printToStderr() const {
  for (const Diagnostic& d : errors_) {
    std::cerr << d.file << ":" << d.loc.line << ":" << d.loc.column << ": error: " << d.message << "\n";
  }
}

}  // namespace lorsc

