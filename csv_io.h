#pragma once
#include <ostream>
#include <string>
#include <vector>

// CSV writer with RFC 4180-compatible quoting for commas, quotes and newlines.
std::string csv_escape(const std::string &value);
void csv_row(std::ostream &output, const std::vector<std::string> &columns);
