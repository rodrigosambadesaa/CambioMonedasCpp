#pragma once

#include <optional>
#include <string>

// Reference rates from Frankfurter v2 are not intraday market quotes.
// Returns nullopt when this build lacks libcurl or the remote reply is invalid.
std::optional<std::string> fetch_reference_rate(const std::string &from_iso,
                                                const std::string &to_iso);
