#include "./parse_data.hpp"

#include <charconv>
#include <stdexcept>
#include <string>

double json5::detail::parse_double(std::string_view spelling) {
    /// XXX: GCC 9 is missing from_chars for floating point types. Fall back to stod
    return std::stod(std::string(spelling));
    /// XXX: Impl for from_chars:
    // using namespace std::string_literals;
    // double ret = 0;
    // auto   res = std::from_chars(spelling.data(), spelling.data() + spelling.size(), ret);
    // if (res.ec == std::errc::result_out_of_range) {
    //     throw std::range_error("Number value '"s + std::string(spelling) + "' is too large");
    // } else if (res.ec == std::errc::invalid_argument) {
    //     throw std::invalid_argument("Number value string '"s + std::string(spelling)
    //                                 + "' is not a valid number");
    // } else if (res.ptr != spelling.data() + spelling.size()) {
    //     throw std::invalid_argument("Number string '"s + std::string(spelling)
    //                                 + "' has trailing characters");
    // } else {
    //     return ret;
    // }
}

void json5::detail::throw_error(std::string_view message, token tok) {
    std::string what = "Error at input line " + std::to_string(tok.line) + ", column "
        + std::to_string(tok.column) + " (Token ‘" + std::string(tok.spelling)
        + "’): " + std::string(message);
    throw std::runtime_error(what);
}