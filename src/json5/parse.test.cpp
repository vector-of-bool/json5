#include <json5/parse.hpp>

#include <catch2/catch.hpp>

using pek = json5::parse_event::kind_t;

struct event_case {
    pek              expect_kind;
    std::string_view expect_spelling;
};

void check_parse(std::string_view str, std::vector<event_case> cases) {
    json5::parser p{str};

    INFO("Parsing string: " << str);

    auto ev_it  = p.begin();
    auto exp_it = cases.begin();
    for (; ev_it != p.end() && exp_it != cases.end(); ++exp_it) {
        CHECK(exp_it->expect_kind == ev_it->kind);
        CHECK(exp_it->expect_spelling == ev_it->token.spelling);
        CHECK(p.error_message() == "");
        INFO("Just saw token: " << ev_it->token.spelling);
        ++ev_it;
    }
    if (ev_it != p.end()) {
        INFO("Pending token kind: " << ev_it->token.kind);
        INFO("Pending token: " << ev_it->token.spelling);
        FAIL_CHECK("Extra unexpected parser events");
    }
    if (exp_it != cases.end()) {
        INFO("Expected token: " << exp_it->expect_spelling);
        FAIL_CHECK("Expected more parser events than were received");
    }
}

TEST_CASE("Simple parse") {
    check_parse("null", {{pek::null_literal, "null"}, {pek::eof, ""}});
    check_parse("1.2", {{pek::number_literal, "1.2"}, {pek::eof, ""}});
    check_parse("'foo'", {{pek::string_literal, "'foo'"}, {pek::eof, ""}});
    check_parse("\"string\"", {{pek::string_literal, "\"string\""}, {pek::eof, ""}});
    check_parse("true", {{pek::boolean_literal, "true"}, {pek::eof, ""}});
    check_parse("/* ignore comment */ true", {{pek::boolean_literal, "true"}, {pek::eof, ""}});
    check_parse("true // Trailing comment", {{pek::boolean_literal, "true"}, {pek::eof, ""}});
}

TEST_CASE("Arrays") {
    check_parse("[]",
                {
                    {pek::array_begin, "["},
                    {pek::array_end, "]"},
                    {pek::eof, ""},
                });

    check_parse("[[]]",
                {
                    {pek::array_begin, "["},
                    {pek::array_begin, "["},
                    {pek::array_end, "]"},
                    {pek::array_end, "]"},
                    {pek::eof, ""},
                });

    check_parse("[[],]",
                {
                    {pek::array_begin, "["},
                    {pek::array_begin, "["},
                    {pek::array_end, "]"},
                    {pek::array_end, "]"},
                    {pek::eof, ""},
                });

    auto strings = {
        "[true]",
        "[true, ]",
        "[true,]",
        "[true, /* Comment */]",
        "[true /* Comment */]",
        "[true /* Comment */, ]",
        "[true /* Comment */]",
        "[/* Comment */ true]",
        "[/* Comment */\n true // Stuff\n]",
    };
    for (auto str : strings) {
        check_parse(str,
                    {
                        {pek::array_begin, "["},
                        {pek::boolean_literal, "true"},
                        {pek::array_end, "]"},
                        {pek::eof, ""},
                    });
    }
}

TEST_CASE("Objects") {
    check_parse("{}",
                {
                    {pek::object_begin, "{"},
                    {pek::object_end, "}"},
                    {pek::eof, ""},
                });

    check_parse("{foo: 1}",
                {
                    {pek::object_begin, "{"},
                    {pek::object_key, "foo"},
                    {pek::number_literal, "1"},
                    {pek::object_end, "}"},
                    {pek::eof, ""},
                });

    check_parse("{foo: {},}",
                {
                    {pek::object_begin, "{"},
                    {pek::object_key, "foo"},
                    {pek::object_begin, "{"},
                    {pek::object_end, "}"},
                    {pek::object_end, "}"},
                    {pek::eof, ""},
                });

    auto strings = {
        "{foo: 2.2}",
        "{foo: 2.2,}",
        "{/* Comment */ foo: 2.2}",
        "{/* Comment */ foo: 2.2, }",
        "{/* Comment */ foo /* bar */: 2.2, }",
        "{/* Comment */ foo /* bar */ : /* baz */ 2.2, }",
    };

    for (auto s : strings) {
        check_parse(s,
                    {
                        {pek::object_begin, "{"},
                        {pek::object_key, "foo"},
                        {pek::number_literal, "2.2"},
                        {pek::object_end, "}"},
                        {pek::eof, ""},
                    });
    }
}

TEST_CASE("Resumable") {
    check_parse("[1, 2, 3] /* Comment */ [1, 2, 3]",
                {
                    {pek::array_begin, "["},
                    {pek::number_literal, "1"},
                    {pek::number_literal, "2"},
                    {pek::number_literal, "3"},
                    {pek::array_end, "]"},
                    {pek::array_begin, "["},
                    {pek::number_literal, "1"},
                    {pek::number_literal, "2"},
                    {pek::number_literal, "3"},
                    {pek::array_end, "]"},
                    {pek::eof, ""},
                });
}

void check_reject(json5::parser& p, std::string_view expect_message) {
    INFO("Expecting message: " << expect_message);
    for (auto ev : p) {
        if (ev.kind == json5::parse_event::eof) {
            FAIL_CHECK("End-of-file reached without generating an error");
        }
        if (ev.kind == json5::parse_event::invalid) {
            CHECK(p.error_message() == expect_message);
            break;
        }
    }
}

void check_reject(std::string_view str, std::string_view expect_message) {
    INFO("Parsing bad string: " << str);
    json5::parser p{str};
    check_reject(p, expect_message);
}

TEST_CASE("Reject") {
    check_reject(".[{{A", "Invalid token");
    check_reject("{", "Unterminated object literal");
    check_reject("[", "Unterminated array literal");
    check_reject("[12, ", "Unterminated array literal");
    check_reject("[12", "Unterminated array literal");
    check_reject("/* bad comment", "Unterminated block comment");
    check_reject("{12: 'string'}", "Object member keys must be strings or identifiers.");
    check_reject("['foo',,]", "Extraneous `,` in array literal.");
    check_reject("{'foo': ,}", "Expected value before `,` in object literal.");
    check_reject("{'foo': 12,,}", "Extraneous `,` in object literal.");
    check_reject("[,]", "Extraneous `,` in array literal.");
    check_reject("foo", "An object key identifier is not a valid value.");
}

TEST_CASE("Reject restricted opts") {
    json5::parse_options opts;

    using toggle                = json5::toggle;
    opts.c_comments             = GENERATE(toggle::on, toggle::off);
    opts.trailing_commas         = GENERATE(toggle::on, toggle::off);
    opts.bare_ident_keys        = GENERATE(toggle::on, toggle::off);
    opts.single_quote_strings    = GENERATE(toggle::on, toggle::off);
    opts.escape_newline_strings = GENERATE(toggle::on, toggle::off);

    struct case_ {
        std::string_view given;
        json5::toggle    toggle_value;
        std::string_view error_message = "";
    };
    case_ cases[] = {
        {.given         = "[1, 2, /* comment */ 3]",
         .toggle_value  = opts.c_comments,
         .error_message = "Comments are not allowed."},
        {.given         = "[1, 2, 3,]",
         .toggle_value  = opts.trailing_commas,
         .error_message = "Trailing commas are not allowed: Expected an array value."},
        {.given         = "{\"foo\": true,}",
         .toggle_value  = opts.trailing_commas,
         .error_message = "Trailing commas are not allowed: Expected an object key."},
        {.given         = "{foo: true}",
         .toggle_value  = opts.bare_ident_keys,
         .error_message = "Bare identifier object keys are not allowed."},
        {.given         = "{'foo': true}",
         .toggle_value  = opts.single_quote_strings,
         .error_message = "Single-quote strings are not allowed."},
        {.given         = "{\"foo\": 'bar'}",
         .toggle_value  = opts.single_quote_strings,
         .error_message = "Single-quote strings are not allowed."},
        {.given         = "\"foo\\\nbar\"",
         .toggle_value  = opts.escape_newline_strings,
         .error_message = "Escaped newlines in strings are not allowed."},
        {.given         = "{\"foo\\\nbar\": true}",
         .toggle_value  = opts.escape_newline_strings,
         .error_message = "Escaped newlines in strings are not allowed."},
    };

    for (const auto [given, tgl, error] : cases) {
        CAPTURE(given);
        json5::parser p{given, opts};
        if (tgl == json5::toggle::on) {
            // We expect this feature to be enabled
            for (auto ev : p) {
                CHECK(ev.kind != ev.invalid);
            }
        } else {
            check_reject(p, error);
        }
    }
}