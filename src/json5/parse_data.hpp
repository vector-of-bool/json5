#pragma once

#include <json5/data.hpp>
#include <json5/parse.hpp>

namespace json5 {

template <typename Data = data>
Data parse_next_value(parser& p);

namespace detail {

template <typename Data>
Data parse_inner(json5::parser& p, const json5::parse_event& ev);

double parse_double(std::string_view);

[[noreturn]] void throw_error(std::string_view message, token tok);

template <typename T>
T realize_number(token tok) {
    return T(parse_double(tok.spelling));
}

template <typename T>
T realize_boolean(token tok) {
    if (tok.spelling == "true") {
        return T(true);
    } else {
        return T(false);
    }
}

template <typename String>
String realize_string(token tok) {
    auto spelling = tok.spelling;
    if (spelling.size() < 2) {
        throw_error("Invalid string token", tok);
    }

    auto       it   = spelling.begin();
    const auto stop = spelling.end();

    char quote = *it;
    ++it;  // Skip the quote

    String ret;
    bool   escaped = false;
    for (; it != stop; ++it) {
        char c = *it;
        if (escaped) {
            switch (c) {
            case '"':
            case '\'':
            case '\\':
                ret.push_back(c);
                break;
            case 'n':
                ret.push_back('\n');
                break;
            case 'r':
                ret.push_back('\r');
                break;
            case '\n':
                // An escaped newline: Just ignore it like it doesn't exist
                break;
            }
            escaped = false;
            continue;
        } else if (c == '\\') {
            escaped = true;
            continue;
        } else if (c == quote) {
            break;
        } else {
            ret.push_back(c);
        }
    }
    if (it == stop || (std::next(it) != stop)) {
        throw_error("Invalid string token", tok);
    }
    return ret;
}

template <typename Array>
Array parse_array_inner(json5::parser& p) {
    Array ret;
    for (auto ev = p.next(); ev.kind != ev.array_end; ev = p.next()) {
        ret.push_back(parse_inner<typename Array::value_type>(p, ev));
    }
    return ret;
}

template <typename Object>
Object parse_object_inner(json5::parser& p) {
    Object ret;
    using key_type    = typename Object::key_type;
    using mapped_type = typename Object::mapped_type;
    for (auto ev = p.next(); ev.kind != ev.object_end; ev = p.next()) {
        if (ev.kind != ev.object_key) {
            throw_error(p.error_message(), ev.token);
        }
        // Get that key!
        key_type new_key;
        auto     key_tok = ev.token;
        if (key_tok.kind == token::identifier) {
            new_key = key_type(key_tok.spelling);
        } else if (key_tok.kind == token::string_literal) {
            new_key = realize_string<key_type>(key_tok);
        } else {
            throw_error("Invalid object member key token", key_tok);
        }

        // Get the corresponding value
        mapped_type new_val = parse_next_value<mapped_type>(p);

        ret.emplace(std::move(new_key), std::move(new_val));
    }
    return ret;
}

template <typename Data>
Data parse_inner(json5::parser& p, const json5::parse_event& ev) {
    using string_type  = typename Data::string_type;
    using number_type  = typename Data::number_type;
    using null_type    = typename Data::null_type;
    using boolean_type = typename Data::boolean_type;
    using array_type   = typename Data::array_type;
    using object_type  = typename Data::object_type;

    using pek = parse_event::kind_t;
    switch (ev.kind) {
    case pek::number_literal:
        return realize_number<number_type>(ev.token);
    case pek::boolean_literal:
        return realize_boolean<boolean_type>(ev.token);
    case pek::string_literal:
        return realize_string<string_type>(ev.token);
    case pek::null_literal:
        return null_type();
    case pek::invalid:
        throw_error(p.error_message(), ev.token);
    case pek::eof:
        throw_error("Unexpected end-of-input", ev.token);
    case pek::array_begin:
        return parse_array_inner<array_type>(p);
    case pek::object_begin:
        return parse_object_inner<object_type>(p);
    default:
        throw_error("Invalid parse event sequence", ev.token);
    }
}

}  // namespace detail

template <typename Data>
Data parse_next_value(parser& p) {
    return detail::parse_inner<Data>(p, p.next());
}

template <typename Data = data>
Data parse_data(std::string_view str) {
    parser p{str};
    auto   v      = parse_next_value(p);
    auto   eof_ev = p.next();
    if (eof_ev.kind != eof_ev.eof) {
        detail::throw_error("Trailing characters in JSON data", eof_ev.token);
    }
    return v;
}

}  // namespace json5