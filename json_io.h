#pragma once
#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <vector>

using CurrencyBlocks = std::map<std::string, std::vector<std::string>>;

void export_stock_json_cpp(std::ostream &out, const CurrencyBlocks &denominations,
                           const CurrencyBlocks &stocks,
                           const std::function<bool(const std::string &)> &valid_currency);
