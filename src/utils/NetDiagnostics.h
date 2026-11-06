#pragma once
#include "ConfigStore.h"

// Diagnostic builds only: choose a clean test topology before Wi-Fi starts.
bool beginNetDiagnostics(const RepeaterConfig& config);
void loopNetDiagnostics();
