#include "exchange_api.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

#ifdef USE_LIBCURL
#include <curl/curl.h>
#endif

namespace {
bool iso(const std::string &code) {
    return code.size() == 3 && std::all_of(code.begin(), code.end(),
        [](unsigned char c) { return c >= 'A' && c <= 'Z'; });
}

#ifdef USE_LIBCURL
struct Reply { std::string body; };
size_t receive(char *data, size_t size, size_t count, void *context) {
    if (size != 0 && count > static_cast<size_t>(-1) / size) return 0;
    const size_t n = size * count;
    Reply *reply = static_cast<Reply *>(context);
    if (n > 16384 || reply->body.size() > 16384 - n) return 0;
    reply->body.append(data, n);
    return n;
}

std::optional<std::string> unique_value(const std::string &json,
                                        const std::string &key, bool number) {
    const std::regex pattern(number ?
        "\\\"" + key + "\\\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)" :
        "\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::sregex_iterator current(json.begin(), json.end(), pattern), end;
    if (current == end) return std::nullopt;
    std::string value = (*current)[1].str();
    if (++current != end) return std::nullopt; // Ambiguous duplicate key.
    return value;
}

std::optional<std::string> parse_rate(const std::string &json,
                                      const std::string &from,
                                      const std::string &to) {
    if (json.empty() || json.size() > 16384 ||
        json.find('{') == std::string::npos || json.find('}') == std::string::npos)
        return std::nullopt;
    if (unique_value(json, "base", false) != std::optional<std::string>(from) ||
        unique_value(json, "quote", false) != std::optional<std::string>(to))
        return std::nullopt;
    auto rate = unique_value(json, "rate", true);
    if (!rate || rate->size() > 100 ||
        std::all_of(rate->begin(), rate->end(), [](char c) { return c == '0' || c == '.'; }))
        return std::nullopt;
    return rate;
}
#endif
}

std::optional<std::string> fetch_reference_rate(const std::string &from_iso,
                                                const std::string &to_iso) {
    if (!iso(from_iso) || !iso(to_iso)) return std::nullopt;
    if (from_iso == to_iso) return std::string("1");
#ifdef USE_LIBCURL
    CURL *handle = curl_easy_init();
    if (!handle) return std::nullopt;
    Reply reply;
    const std::string url = "https://api.frankfurter.dev/v2/rate/" +
                            from_iso + "/" + to_iso;
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &reply);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "ProgVorazCpp/1.0");
    CURLcode result = curl_easy_perform(handle);
    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(handle);
    return result == CURLE_OK && status == 200 ?
        parse_rate(reply.body, from_iso, to_iso) : std::nullopt;
#else
    return std::nullopt;
#endif
}
