#include "./parse.hpp"

#include <cassert>

using namespace json5;

namespace json5::detail {

/**
 * This is the main implementation of the event-based streaming JSON5 parser.
 * It consumes a stream of tokens. Refer to the grammer document:
 *
 *      https://spec.json5.org/#syntactic-grammar
 *
 * Zero-alloc nesting tracking
 * ===========================
 *
 * We perform no allocations and track our object/array nesting in an array of
 * bits, with the leading bit set to `1` to indicate that we are in an object,
 * and a leading `0` to indicate that we are in an array. When we enter an
 * array or object, all bits are shifted right and the lead bit is set to "push"
 * our new state. When we exit an array/object, the bits are shifted left to
 * restore the prior state. We keep track of the nesting depth in a separate
 * integral value. When that depth is zero, we are at the root of a JSON5
 * document.
 *
 * Because the length of the bit array is fixed, this imposes a limitation in
 * the nesting depth that can be tracked by the parser. Exceeding this limit
 * will produce an error event.
 */

struct parser_impl {
    parser& self;

    /// Obtain the current token
    token curtok() noexcept { return self._toks.current(); }
    /// Obtain the current token kind
    token::kind_t kind() noexcept { return self._toks.current_kind(); }

    /// Test if we are parsing an object literal
    bool in_object() const noexcept { return self._nest_depth && self._nest_flag_bits[0]; }
    /// Test if we are parsing an array literal
    bool in_array() const noexcept { return self._nest_depth && !self._nest_flag_bits[0]; }

    /// Set the next parser state
    void become(parser::state_t st) noexcept { self._state = st; }

    /// Generate a value event and transition to the next state based on our
    /// current parsing context
    parse_event value(parse_event::kind_t k) noexcept {
        if (in_object()) {
            // We need to parse an object tail next
            become(self.object_tail);
        } else if (in_array()) {
            // We need to parse an array tail next
            become(self.array_tail);
        } else {
            // We finished parsing the top-level JSON5 value, so end
            become(self.top);
        }
        // Return that value:
        return parse_event{k, curtok()};
    }

    /// Set the error message and return an error event
    parse_event fail(std::string_view error_message) noexcept {
        self._error_message = error_message;
        return parse_event{parse_event::invalid, curtok()};
    }

    /*
    ########     ###    ########   ######  ########
    ##     ##   ## ##   ##     ## ##    ## ##
    ##     ##  ##   ##  ##     ## ##       ##
    ########  ##     ## ########   ######  ######
    ##        ######### ##   ##         ## ##
    ##        ##     ## ##    ##  ##    ## ##
    ##        ##     ## ##     ##  ######  ########
    */
    // Return the next parser event
    parse_event parse_next() noexcept {
        // Advance one token,
        self._toks.advance();
        // And skip all comments. They have no effect on parser state.
        while (kind() == token::comment) {
            self._toks.advance();
        }

        if (kind() == token::unterm_comment) {
            return fail("Unterminated block comment");
        }

        // If the token emitter has nothing more, then we have nothing more.
        if (self._toks.done()) {
            self._done = true;
            return {};
        }

        // If the tokenizer emitted an EOF, then we have just reached the end of our input, and we
        // have no more events to deliver
        if (kind() == token::eof && self._state == self.top) {
            return parse_event{parse_event::eof, curtok()};
        }

        /// The main body of parsing
        switch (self._state) {

        // Top-level parsing. Parse a value
        case parser::top:
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

        // Parse a colon for the object member, then parse the value for the member
        case parser::object_kv_colon: {
            if (kind() != token::punct_colon) {
                return fail("Expected `:` following object member key");
            }
            become(self.object_value);
            return parse_next();
        }

        // Parse the value for an object
        case parser::object_value:
            return parse_value();

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
        switch (kind()) {
        // A closing bracket `]`: End of the array
        case token::punct_bracket_close:
            return array_end();
        case token::eof:
            return fail("Unterminated array literal");
        default:
            // The only alternative is to parse a value
            return parse_value();
        }
    }

    /**
     * Parse an object element, or the closing of an object. This state appears after an opening `{`
     * or a continuing comma `,`.
     */
    parse_event parse_obj_elem() noexcept {
        switch (kind()) {
        /// A closing brace
        case token::punct_brace_close:
            return object_end();
        /// The member may be either an identifier or a string literal
        case token::identifier:
        case token::string_literal:
            become(self.object_kv_colon);
            return parse_event{parse_event::object_key, curtok()};
        /// Unexpected end-of-file
        case token::eof:
            return fail("Unterminated object literal");
        case token::number_literal:
        case token::boolean_literal:
        case token::null_literal:
        case token::punct_brace_open:
        case token::punct_bracket_open:
            return fail("Object member keys must be strings or identifiers.");
        case token::punct_comma:
            return fail("Extraneous `,` in object literal.");
        /// Any other token is not allowed
        default:
            return fail("Expected an object member or closing brace `}`");
        }
    }

    /**
     * Parse an array "tail," either a closing bracket `]` or a comma `,`. This state appears after
     * parsing an array element value.
     */
    parse_event parse_array_tail() noexcept {
        switch (kind()) {
        /// A closing bracket
        case token::punct_bracket_close:
            return array_end();
        /// A comma, so we should now parse another value or a closing
        /// bracket `]`
        case token::punct_comma:
            become(self.array_value_or_close);
            return parse_next();
        /// Unexpected end-of-file
        case token::eof:
            return fail("Unterminated array literal");
        /// Anything else is invalid
        default:
            return fail("Expected `,` or `]` in array");
        }
    }

    /**
     * Parse an object "tail," either a closing brace `}` or a comma `,`. This state appears after
     * parsing an object member value.
     */
    parse_event parse_obj_tail() noexcept {
        switch (kind()) {
        /// A comma should be followed by another object key or a closing `}`
        case token::punct_comma:
            become(self.object_key_or_close);
            return parse_next();
        /// A closing brace ends the object
        case token::punct_brace_close:
            return object_end();
        // Unexpected end-of-file
        case token::eof:
            return fail("Unterminated object literal");
        /// Nothing else allowed
        default:
            return fail("Expected `,` or `}` in object");
        }
    }

    /**
     * Parse a JSON5 value.
     */
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
            return fail("An object key identifier is not a valid value.");
        case tok.punct_bracket_close:
            return fail("Unexpected closing `]`");
        case tok.punct_brace_close:
            return fail("Unexpected closing `}`");

        case tok.unterm_string:
            return fail("Unterminated string");
        case tok.punct_colon:
            return fail("Unexpected `:`");
        case tok.punct_comma:
            if (in_array()) {
                return fail("Extraneous `,` in array literal.");
            } else if (in_object()) {
                return fail("Expected value before `,` in object literal.");
            } else {
                return fail("Unexpected `,`");
            }

        case tok.invalid:
            return fail("Invalid token");
        case tok.comment:
        case tok.unterm_comment:
            assert(false && "Unreachable (comment token)");
            break;
        }
        assert(false && "Unreachable");
        std::terminate();
    }

    void pop_state() noexcept {
        assert(self._nest_depth > 0);
        --self._nest_depth;
        self._nest_flag_bits <<= 1;
        if (in_object()) {
            become(self.object_tail);
        } else if (in_array()) {
            become(self.array_tail);
        } else {
            become(self.top);
        }
    }

    parse_event array_begin() noexcept {
        if (self._nest_depth == self._nest_flag_bits.size()) {
            return fail("Array/object nesting is too deep.");
        }
        ++self._nest_depth;
        self._nest_flag_bits >>= 1;
        self._nest_flag_bits[0] = 0;
        // We always set our new state to be to expect an array value
        become(self.array_value_or_close);
        return parse_event{parse_event::array_begin, curtok()};
    }

    parse_event array_end() noexcept {
        pop_state();
        return parse_event{parse_event::array_end, curtok()};
    }

    parse_event object_begin() noexcept {
        if (self._nest_depth == self._nest_flag_bits.size()) {
            return fail("Array/object nesting is too deep.");
        }
        ++self._nest_depth;
        self._nest_flag_bits >>= 1;
        self._nest_flag_bits[0] = 1;
        // We always set our new state to be to expect an object member
        become(self.object_key_or_close);
        return parse_event{parse_event::object_begin, curtok()};
    }

    parse_event object_end() noexcept {
        pop_state();
        return parse_event{parse_event::object_end, curtok()};
    }
};

}  // namespace json5::detail

parse_event parser::next() noexcept { return detail::parser_impl{*this}.parse_next(); }