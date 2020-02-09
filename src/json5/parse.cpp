#include "./parse.hpp"

#include <cassert>

using namespace json5;

namespace json5::detail {

struct parser_impl {
    parser& self;

    // Obtain the current token
    token curtok() noexcept { return self._toks.current(); }
    /// Obtain the current token kind
    token::kind_t kind() noexcept { return self._toks.current_kind(); }

    // Set the error message and return an error event
    parse_event fail(std::string_view error_message) noexcept {
        self._error_message = error_message;
        return parse_event{parse_event::invalid, curtok()};
    }

    // Test if we are parsing an object literal
    bool in_object() const noexcept { return self._nest_depth && self._nest_flag_bits[0]; }
    // Test if we are parsing an array literal
    bool in_array() const noexcept { return self._nest_depth && !self._nest_flag_bits[0]; }

    // Generate a value event and transition to the next state based on our
    // current parsing context
    parse_event value(parse_event::kind_t k) noexcept {
        if (in_object()) {
            // We need to parse an object tail next
            self._state = self.object_tail;
        } else if (in_array()) {
            // We need to parse an array tail next
            self._state = self.array_tail;
        } else {
            // We finished parsing the top-level JSON5 value, so end
            self._state = self.init;
        }
        // Return that value:
        return parse_event{k, curtok()};
    }

    // Return the next parser event
    parse_event parse_next() noexcept {
        // Advance one token,
        self._toks.advance();
        // And skip all comments. They have no effect on parser state.
        while (kind() == token::comment) {
            self._toks.advance();
        }

        // If the token emitter has nothing more, then we have nothing more.
        if (self._toks.done()) {
            self._done = true;
            return {};
        }

        // If the tokenizer emitted an EOF, then we have just reached the end of our input, and we
        // have no more events to deliver
        if (kind() == token::eof && self._state == self.init) {
            return parse_event{parse_event::eof, curtok()};
        }

        // The main body of parsing
        switch (self._state) {

            // Top-level parsing. Parse a value
        case parser::init:
            return parse_value();

            // Parse either an array value, or a closing `]`
        case parser::array_value_or_close:
            return parse_array_elem();

            // Parse either a comma `,` or a closing `]`
        case parser::array_tail:
            return parse_array_tail();

            // Parse either an object key, or a closing `}`
        case parser::object_key_or_close:
            return parse_obj_elem();

            // Parse the value for an object
        case parser::object_value:
            return parse_obj_value();

            // Parse either a comma `,` or a closing `}`
        case parser::object_tail:
            return parse_obj_tail();
        }

        assert(false && "Unreachable");
        std::terminate();
    }

    /**
     * Parses an array element, or the closing of an array. This state appears
     * after an opening `[` or after a continuing comma `,`.
     */
    parse_event parse_array_elem() noexcept {
        if (kind() == token::punct_bracket_close) {
            // A closing bracket `]`: End of the array
            return array_end();
        } else {
            // The only alternative is to parse a value
            return parse_value();
        }
    }

    /**
     * Parse an array "tail," either a closing bracket `]` or a comma `,`. This
     * state appears after parsing an array element value.
     */
    parse_event parse_array_tail() noexcept {
        if (kind() == token::punct_comma) {
            // We have a comma, so we should now parse another value or a closing
            // bracket `]`
            self._state = self.array_value_or_close;
            return parse_next();
        } else if (kind() == token::punct_bracket_close) {
            return array_end();
        } else {
            return fail("Expected `,` or `]` in array");
        }
    }

    parse_event parse_obj_tail() noexcept {
        if (kind() == token::punct_comma) {
            self._state = self.object_key_or_close;
            return parse_next();
        } else if (kind() == token::punct_brace_close) {
            return object_end();
        } else {
            return fail("Expected `,` or `}` in object");
        }
    }

    parse_event parse_obj_elem() noexcept {
        switch (kind()) {
        case token::punct_brace_close:
            return object_end();
        case token::identifier:
        case token::string_literal:
            self._state = self.object_value;
            return parse_event{parse_event::object_key, curtok()};
        default:
            return fail("Expected an object member or closing brace `}`");
        }
    }

    parse_event parse_obj_value() noexcept {
        if (kind() != token::punct_colon) {
            return fail("Expected `:` following object member name");
        }
        self._toks.advance();
        return parse_value();
    }

    parse_event parse_value() noexcept {
        token tok = self._toks.current();
        switch (tok.kind) {

        // Literals
        case tok.null_literal:
            return value(parse_event::null_literal);
        case tok.boolean_literal:
            return value(parse_event::boolean_literal);
        case tok.string_literal:
            return value(parse_event::string_literal);
        case tok.number_literal:
            return value(parse_event::number_literal);

        // Arrays
        case tok.punct_bracket_open:
            return array_begin();

        // Objects
        case tok.punct_brace_open:
            return object_begin();

        // The end!
        case tok.eof:
            return fail("Unexpected end-of-input: Expected a value");

        // Other error cases
        case tok.identifier:
            return fail("A object key identifier is not a valid value.");
        case tok.punct_bracket_close:
            return fail("Unexpected closing `]`");
        case tok.punct_brace_close:
            return fail("Unexpected closing `}`");

        case tok.unterm_string:
            return fail("Unterminated string");
        case tok.punct_colon:
            return fail("Unexpected `:`");
        case tok.punct_comma:
            return fail("Unexpected `,`");

        case tok.invalid:
        case tok.comment:
            // Unreachable
            break;
        }
        assert(false && "Unreachable");
    }

    parse_event next_nested() noexcept { assert(false && "Unimplemented"); }

    void pop_state() noexcept {
        --self._nest_depth;
        self._nest_flag_bits <<= 1;
        if (in_object()) {
            self._state = self.object_tail;
        } else if (in_array()) {
            self._state = self.array_tail;
        } else {
            self._state = self.init;
        }
    }

    parse_event array_begin() noexcept {
        ++self._nest_depth;
        self._nest_flag_bits >>= 1;
        self._nest_flag_bits[0] = 0;
        // We always set our new state to be to expect an array value
        self._state = self.array_value_or_close;
        return parse_event{parse_event::array_begin, curtok()};
    }

    parse_event array_end() noexcept {
        pop_state();
        return parse_event{parse_event::array_end, curtok()};
    }

    parse_event object_begin() noexcept {
        ++self._nest_depth;
        self._nest_flag_bits >>= 1;
        self._nest_flag_bits[0] = 1;
        self._state             = self.object_key_or_close;
        return parse_event{parse_event::object_begin, curtok()};
    }

    parse_event object_end() noexcept {
        pop_state();
        return parse_event{parse_event::object_end, curtok()};
    }
};

}  // namespace json5::detail

parse_event parser::next() noexcept { return detail::parser_impl{*this}.parse_next(); }