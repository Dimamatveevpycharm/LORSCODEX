#pragma once

#include <string>
#include <vector>

#include "lorsc/SourceLocation.h"

namespace lorsc {

struct Diagnostic {
  std::string file;
  SourceLocation loc;
  std::string message;
};

class DiagnosticsEngine {
 public:
  void reportError(const std::string& file, SourceLocation loc, const std::string& message);
  void reportParseError(const std::string& file, int line, int column, const std::string& message);

  bool hasErrors() const { return !errors_.empty(); }
  std::size_t errorCount() const { return errors_.size(); }
  const std::vector<Diagnostic>& errors() const { return errors_; }
  void printToStderr() const;

 private:
  std::vector<Diagnostic> errors_;
};

}  // namespace lorsc


