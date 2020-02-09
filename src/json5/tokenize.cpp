#include <json5/tokenize.hpp>

#include <cassert>
#include <cctype>

using namespace json5;

/*
##    ##  #######  ######## ########  ##
###   ## ##     ##    ##    ##       ####
####  ## ##     ##    ##    ##        ##
## ## ## ##     ##    ##    ######
##  #### ##     ##    ##    ##        ##
##   ### ##     ##    ##    ##       ####
##    ##  #######     ##    ########  ##
*/
/**
 * This lexical grammer is *mostly* complete, but is missing a few features.
 * For reference, use the https://spec.json5.org/#lexical-grammar page.
 *
 * The following features are known to be missing:
 *  - Using std::isspace for white-space checks is probably missing a few.
 *  - Identifiers may use non-ASCII characters. This only handles the basics.
 *  - Missing negative and positive `Infinity` and `NaN` literals.
 *  - Doesn't respect line separator (U-2028) or paragraph separator (U-2029)
 *    as line endings.
 *  - Incomplete escape sequence handling.
 */

namespace {

bool is_space(char c) noexcept { return std::isspace(c); }
bool is_ident_first(char c) noexcept { return std::isalpha(c) || c == '_' || c == '$'; }
bool is_ident_char(char c) noexcept { return is_ident_first(c) || std::isdigit(c); }
bool is_line_term(char c) { return c == '\n' || c == '\r'; }

}  // namespace

char tokenizer::_peek(int n) const noexcept {
    auto remaining = _full_buffer.end() - _head;
    if (remaining <= n) {
        return '\0';
    }
    return *(_head + n);
}

void tokenizer::_take(std::size_t n) noexcept {
    for (; n != 0; --n) {
        ++_next_column;
        if (*_head == '\n') {
            _next_column = 0;
            ++_next_line_no;
        }
        ++_head;
    }
}

void tokenizer::_adv_ident() noexcept {
    while (_head != _full_buffer.end() && is_ident_char(_peek(0))) {
        _take(1);
    }

    _current_kind = token::identifier;
    auto str      = current_string();
    if (str == "null") {
        _current_kind = token::null_literal;
    } else if (str == "false" || str == "true") {
        _current_kind = token::boolean_literal;
    } else if (str == "Infinity" || str == "NaN") {
        _current_kind = token::number_literal;
    }
}

void tokenizer::_adv_line_comment() noexcept {
    while (_head != _full_buffer.end() && !is_line_term(*_head)) {
        _take(1);
    }
    _current_kind = token::comment;
}

void tokenizer::_adv_block_comment() noexcept {
    bool terminated = false;
    while (_head != _full_buffer.end()) {
        if (_peek(0) == '*' && _peek(1) == '/') {
            _take(2);
            terminated = true;
            break;
        }
        _take(1);
    }
    _current_kind = terminated ? token::comment : token::unterm_comment;
}

void tokenizer::advance() noexcept {
    // Tokenize a string
    auto adv_string = [this](char quote) {
        bool escaped = false;
        while (_head != _full_buffer.end()) {
            if (escaped) {
                // Take the character, no matter what it is
                _take(1);
                escaped = false;
            } else if (*_head == '\\') {
                _take(1);
                escaped = true;
            } else if (*_head == quote) {
                // Closed quote!
                break;
            } else if (is_line_term(*_head)) {
                // BAD! Embedded newline
                break;
            } else {
                // A string character
                _take(1);
            }
        }
        if (_head == _full_buffer.end() || is_line_term(*_head)) {
            // We reached the end of the string without a closing quote.
            _current_kind = token::unterm_string;
        } else {
            _take(1);
            _current_kind = token::string_literal;
        }
    };

    // Tokenize a number literal.
    auto adv_number = [&] {
        _current_kind = token::number_literal;
        while (_head != _full_buffer.end() && std::isdigit(*_head)) {
            _take(1);
        }
        if (*_head == '.' && std::isdigit(_peek(1))) {
            _take(1);
            while (_head != _full_buffer.end() && std::isdigit(*_head)) {
                _take(1);
            }
        }
    };

    // We should not be called a second time after having passed the EOF
    assert(!_done && "advance() called on finished tokenizer");

    /// Skip whitespace
    while (_head != _full_buffer.end() && is_space(_peek(0))) {
        _take(1);
    }

    // Reset attributes for new token
    _tail    = _head;
    _line_no = _next_line_no;
    _column  = _next_column;

    // Check if we've reached the end of the input
    if (_head == _full_buffer.end()) {
        // If we've previous set the token to EOF, then we've already yielded the EOF token
        if (_current_kind == token::eof) {
            // Mark that we've yielded all tokens from the underlying string.
            _done = true;
        }
        // Put the EOF token.
        _current_kind = token::eof;
        return;
    }

    // Get the current character
    char c = _peek(0);

    // Tokenize punctuation:
    switch (c) {
    case '{': {
        _current_kind = token::punct_brace_open;
        _take(1);
        return;
    }
    case '}': {
        _current_kind = token::punct_brace_close;
        _take(1);
        return;
    }
    case '[': {
        _current_kind = token::punct_bracket_open;
        _take(1);
        return;
    }
    case ']': {
        _current_kind = token::punct_bracket_close;
        _take(1);
        return;
    }
    case ':': {
        _current_kind = token::punct_colon;
        _take(1);
        return;
    }
    case ',': {
        _current_kind = token::punct_comma;
        _take(1);
        return;
    }
    }

    if (is_ident_first(c)) {
        // We're seeing an identifier
        _adv_ident();
    } else if (c == '/' && _peek(1) == '/') {
        // Consume a line-comment
        _adv_line_comment();
    } else if (c == '/' && _peek(1) == '*') {
        // Consume a block-comment
        _adv_block_comment();
    } else if (c == '\'' || c == '"') {
        // This is a string literal
        _take(1);
        adv_string(c);
    } else if (std::isdigit(c) || c == '.' || c == '+' || c == '-') {
        // This is a number literal
        if (c == '+' || c == '-') {
            _take(1);
        }
        adv_number();
    } else {
        _current_kind = token::invalid;
        _take(1);
    }
}