#include "json_io.h"

#include <iomanip>

// All monetary values are written as decimal strings (denominations) or
// arbitrary-precision integer tokens (stock), without passing through double.
namespace {
void json_string(std::ostream &out, const std::string &value) {
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    static constexpr char hex[] = "0123456789abcdef";
                    out << "\\u00" << hex[c >> 4] << hex[c & 15];
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
}
}

void export_stock_json_cpp(std::ostream &out, const CurrencyBlocks &denominations,
                           const CurrencyBlocks &stocks,
                           const std::function<bool(const std::string &)> &valid_currency) {
    out << "{\n  \"monedas\": [\n";
    bool first = true;
    for (const auto &[name, values] : denominations) {
        if (!valid_currency(name)) continue;
        const auto &available = stocks.at(name);
        if (!first) out << ",\n";
        out << "    {\"nombre\":";
        json_string(out, name);
        out << ",\"denominaciones\":[";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) out << ',';
            json_string(out, values[i]);
        }
        out << "],\"stock\":[";
        for (size_t i = 0; i < available.size(); ++i)
            out << (i ? "," : "") << available[i];
        out << "]}";
        first = false;
    }
    out << "\n  ]\n}\n";
}
