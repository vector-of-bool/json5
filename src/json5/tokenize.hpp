#pragma once

#include <cassert>
#include <cctype>
#include <string_view>

namespace json5 {

struct token_end_sentinel {};

struct token {
    std::string_view spelling;
    int              line   = 0;
    int              column = 0;

    enum kind_t {
        invalid,
        unterm_string,
        unterm_comment,

        comment,
        identifier,
        punct_brace_open,
        punct_brace_close,
        punct_bracket_open,
        punct_bracket_close,
        punct_colon,
        punct_comma,

        null_literal,
        number_literal,
        string_literal,
        boolean_literal,

        eof,
    } kind
        = invalid;
};

class tokenizer {
    std::string_view _full_buffer;

    int  _line_no      = 0;
    int  _column       = 0;
    int  _next_line_no = 0;
    int  _next_column  = 0;
    bool _done         = false;

    token::kind_t              _current_kind = token::invalid;
    std::string_view::iterator _tail         = _full_buffer.begin();
    std::string_view::iterator _head         = _tail;

    char _peek(int n) const noexcept;
    void _take(std::size_t n) noexcept;
    void _adv_ident() noexcept;
    void _adv_line_comment() noexcept;
    void _adv_block_comment() noexcept;

public:
    explicit tokenizer(std::string_view buf)
        : _full_buffer(buf) {}

    class token_iterator {
        tokenizer* _t;

    public:
        explicit token_iterator(tokenizer& t)
            : _t{&t} {}

        token_iterator& operator++() noexcept {
            _t->advance();
            return *this;
        }

        friend bool operator!=(token_iterator left, token_end_sentinel) noexcept {
            return !left._t->done();
        }
        friend bool operator!=(token_end_sentinel, token_iterator right) noexcept {
            return !right._t->done();
        }
        friend bool operator==(token_iterator left, token_end_sentinel s) noexcept {
            return !(left != s);
        }
        friend bool operator==(token_end_sentinel s, token_iterator right) noexcept {
            return !(s != right);
        }

        token operator*() const noexcept { return _t->current(); }
    };

    void advance() noexcept;

    bool done() const noexcept { return _done; }

    std::string_view current_string() const noexcept {
        return std::string_view(_tail, static_cast<std::string_view::size_type>(_head - _tail));
    }
    token::kind_t current_kind() const noexcept { return _current_kind; }
    token current() const noexcept { return {current_string(), _line_no, _column, current_kind()}; }

    token eof_at_current() const noexcept { return {"", _line_no, _column, token::eof}; }

    token_iterator begin() noexcept {
        advance();
        return token_iterator{*this};
    }
    token_end_sentinel end() const noexcept { return {}; }
};

}  // namespace json5