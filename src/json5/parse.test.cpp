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

    auto strings = {
        "{foo: 2.2}",
        "{foo: 2.2,}",
        "{/* Comment */ foo: 2.2}",
        "{/* Comment */ foo: 2.2, }",
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
    check_parse("[1, 2, 3] /* Comment */ [1, 2, 3]", {
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