#include "csv_io.h"

std::string csv_escape(const std::string &value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') escaped += '"';
        escaped += c;
    }
    return escaped + "\"";
}

void csv_row(std::ostream &output, const std::vector<std::string> &columns) {
    for (size_t i = 0; i < columns.size(); ++i)
        output << (i ? "," : "") << csv_escape(columns[i]);
    output << '\n';
}
