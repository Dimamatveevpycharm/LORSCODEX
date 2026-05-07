#pragma once

#include <memory>
#include <string>

#include "lorsc/AST.h"
#include "lorsc/Diagnostics.h"

namespace lorsc {

std::unique_ptr<Program> parseFile(const std::string& filePath, DiagnosticsEngine& diagnostics);

}  // namespace lorsc


