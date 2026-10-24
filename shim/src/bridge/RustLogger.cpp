#include "bridge/RustLogger.h"

#include "amm_host.h"

#include <cstdio>
#include <cstring>

namespace amm_shim {

namespace {

constexpr std::string_view TARGET          = "alaydriem_mod_manager::shim";
constexpr unsigned char    FIRST_PRINTABLE = 0x20;
constexpr size_t           ESCAPE_LEN      = 7;

uint8_t const* bytes(std::string_view text) { return reinterpret_cast<uint8_t const*>(text.data()); }

} // namespace

void RustLogger::error(std::string_view msg, std::string_view fields, std::source_location at) {
    write(AMM_LOG_LEVEL_ERROR, msg, fields, at);
}

void RustLogger::warn(std::string_view msg, std::string_view fields, std::source_location at) {
    write(AMM_LOG_LEVEL_WARN, msg, fields, at);
}

void RustLogger::info(std::string_view msg, std::string_view fields, std::source_location at) {
    write(AMM_LOG_LEVEL_INFO, msg, fields, at);
}

void RustLogger::debug(std::string_view msg, std::string_view fields, std::source_location at) {
    write(AMM_LOG_LEVEL_DEBUG, msg, fields, at);
}

std::string RustLogger::quote(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (char c : text) {
        auto u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (u < FIRST_PRINTABLE) {
            char escaped[ESCAPE_LEN + 1]{};
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(u));
            out.append(escaped, std::strlen(escaped));
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

void RustLogger::write(uint8_t level, std::string_view msg, std::string_view fields, std::source_location const& at) {
    std::string_view file = at.file_name();
    amm_host_log(
        level,
        bytes(TARGET),
        TARGET.size(),
        bytes(msg),
        msg.size(),
        bytes(fields),
        fields.size(),
        bytes(file),
        file.size(),
        at.line()
    );
}

} // namespace amm_shim
