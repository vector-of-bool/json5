#include <json5/tokenize.hpp>

#include <catch2/catch.hpp>

using tk = json5::token::kind_t;

struct expected_case {
    tk               expected_kind;
    std::string_view expected_spelling;
};

void check_tokenize(std::string_view str, std::vector<expected_case> expectation) {
    json5::tokenizer tks(str);
    auto             it = tks.begin();

    auto exp_it = expectation.begin();
    for (; it != tks.end() && exp_it != expectation.end(); ++it, ++exp_it) {
        auto tok = *it;
        auto exp = *exp_it;
        CHECK(tok.kind == exp.expected_kind);
        CHECK(tok.spelling == exp.expected_spelling);
    }

    if (exp_it != expectation.end()) {
        FAIL_CHECK("Not enough tokens were parsed.");
    } else {
        REQUIRE(it != tks.end());
        auto eof_tok = *it;
        if (eof_tok.kind != tk::eof) {
            INFO("Next token: " << eof_tok.spelling);
            FAIL_CHECK("Too many tokens were parsed");
        } else {
            CHECK(eof_tok.kind == eof_tok.eof);
            CHECK(eof_tok.spelling == "");

            ++it;
            CHECK(it == tks.end());
        }
    }
}

void check_is(json5::token tok, tk kind, std::string_view spelling) {
    CHECK(tok.kind == kind);
    CHECK(tok.spelling == spelling);
}

void check_next(json5::tokenizer::token_iterator it, tk kind, std::string_view spelling) {
    ++it;
    CHECK(it != json5::token_end_sentinel());
    check_is(*it, kind, spelling);
}

TEST_CASE("Tokenize a buffer") {
    check_tokenize("I am a string",
                   {
                       {tk::identifier, "I"},
                       {tk::identifier, "am"},
                       {tk::identifier, "a"},
                       {tk::identifier, "string"},
                   });
}

TEST_CASE("Tokenize comments") {
    check_tokenize("foo /* comment */ bar",
                   {
                       {tk::identifier, "foo"},
                       {tk::comment, "/* comment */"},
                       {tk::identifier, "bar"},
                   });

    check_tokenize("Line // comment",
                   {
                       {tk::identifier, "Line"},
                       {tk::comment, "// comment"},
                   });
}

TEST_CASE("Whitespace skipping") {
    check_tokenize("   foo   ", {{tk::identifier, "foo"}});
    check_tokenize("     ", {});
}

TEST_CASE("Tokenize strings") {
    check_tokenize("'I am a string'", {{tk::string_literal, "'I am a string'"}});
    check_tokenize("\"I am also a string\"", {{tk::string_literal, "\"I am also a string\""}});

    check_tokenize("'This string has \\' escapes'",
                   {{tk::string_literal, "'This string has \\' escapes'"}});

    // Escaped newline
    check_tokenize("'Multiline\\\nstring'", {{tk::string_literal, "'Multiline\\\nstring'"}});

    // An unterminated string isn't an error, its just a bad token:
    check_tokenize("'This string is missing a quote",
                   {{tk::unterm_string, "'This string is missing a quote"}});
    check_tokenize("'This string has a newline\nin it'",
                   {
                       {tk::unterm_string, "'This string has a newline"},
                       {tk::identifier, "in"},
                       {tk::identifier, "it"},
                       {tk::unterm_string, "'"},
                   });
}

TEST_CASE("Tokenize numbers") {
    check_tokenize("1", {{tk::number_literal, "1"}});
    check_tokenize("12", {{tk::number_literal, "12"}});
    check_tokenize("12 33",
                   {
                       {tk::number_literal, "12"},
                       {tk::number_literal, "33"},
                   });
    check_tokenize("1.2", {{tk::number_literal, "1.2"}});
    check_tokenize(".2", {{tk::number_literal, ".2"}});
    check_tokenize("-2", {{tk::number_literal, "-2"}});
}