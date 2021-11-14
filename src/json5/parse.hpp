#pragma once

#include <json5/tokenize.hpp>

#include <bitset>

namespace json5 {

namespace detail {

struct parser_impl;

}  // namespace detail

enum class toggle {
    off,
    on,
};

struct parse_options {
    toggle c_comments             = toggle::on;
    toggle trailing_commas        = toggle::on;
    toggle bare_ident_keys        = toggle::on;
    toggle single_quote_strings   = toggle::on;
    toggle escape_newline_strings = toggle::on;
};

constexpr inline parse_options json5_options = {};
constexpr inline parse_options jsonc_options = {
    .c_comments             = toggle::on,
    .trailing_commas        = toggle::off,
    .bare_ident_keys        = toggle::off,
    .single_quote_strings   = toggle::off,
    .escape_newline_strings = toggle::off,
};
constexpr inline parse_options json_strict_options = {
    .c_comments             = toggle::off,
    .trailing_commas        = toggle::off,
    .bare_ident_keys        = toggle::off,
    .single_quote_strings   = toggle::off,
    .escape_newline_strings = toggle::off,
};

struct parse_event {
    enum kind_t {
        invalid,

        null_literal,
        number_literal,
        string_literal,
        boolean_literal,

        array_begin,
        array_end,

        object_begin,
        object_key,
        object_end,

        comment,
        eof,
    } kind
        = invalid;

    json5::token token;
};

struct event_end_sentinel {};

class parser {
    tokenizer _toks;
    bool      _done = false;

    std::bitset<1024> _nest_flag_bits;
    std::size_t       _nest_depth = 0;

    std::string_view _error_message;

    parse_options _opts;

    friend struct detail::parser_impl;

    enum state_t {
        top,

        array_value_after_comma,
        array_value_or_close,
        array_tail,

        object_key_after_comma,
        object_key_or_close,
        object_kv_colon,
        object_value,
        object_tail,
    } _state
        = top;

public:
    explicit parser(std::string_view buf, parse_options opts)
        : _toks(buf)
        , _opts(opts) {}

    explicit parser(std::string_view buf)
        : parser(buf, parse_options{}) {}

    class event_iterator {
        parser* _p;

        parse_event _current;

    public:
        explicit event_iterator(parser& p) noexcept
            : _p(&p) {}

        event_iterator& operator++() noexcept {
            _current = _p->next();
            return *this;
        }

        friend bool operator!=(event_iterator left, event_end_sentinel) noexcept {
            return !left._p->done();
        }
        friend bool operator!=(event_end_sentinel, event_iterator right) noexcept {
            return !right._p->done();
        }
        friend bool operator==(event_iterator left, event_end_sentinel s) noexcept {
            return !(left != s);
        }
        friend bool operator==(event_end_sentinel s, event_iterator right) noexcept {
            return !(s != right);
        }

        const parse_event* operator->() const noexcept { return &_current; }
        const parse_event& operator*() const noexcept { return _current; }
    };

    event_iterator begin() noexcept {
        auto it = event_iterator(*this);
        ++it;
        return it;
    }
    event_end_sentinel end() const noexcept { return {}; }

    parse_event next() noexcept;
    bool        done() const noexcept { return _done; }

    std::string_view error_message() const noexcept { return _error_message; }
};

}  // namespace json5