#include <json5/parse_data.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Parse simple values") {
    auto v = json5::parse_data("5");
    CHECK(v == 5);

    v = json5::parse_data("3.3");
    CHECK(v == 3.3);

    v = json5::parse_data("null");
    CHECK(v == nullptr);

    v = json5::parse_data("'string'");
    CHECK(v == "string");

    v = json5::parse_data("'string\\n'");
    CHECK(v == "string\n");
}

TEST_CASE("Parse arrays") {
    auto v = json5::parse_data("[]");
    CHECK(v.is_array());

    CHECK(v == json5::data::array_type());

    v = json5::parse_data("['string']");
    CHECK(v == json5::data::array_type({"string"}));

    v = json5::parse_data("['string', ]");
    CHECK(v == json5::data::array_type({"string"}));

    v = json5::parse_data("['foo', 'bar']");
    CHECK(v == json5::data::array_type({"foo", "bar"}));

    v = json5::parse_data("[3, 'string']");
    CHECK(v == json5::data::array_type({3, "string"}));
}

TEST_CASE("Parse objects") {
    auto v = json5::parse_data("{}");
    CHECK(v.is_object());

    CHECK(v == json5::data::object_type());

    v = json5::parse_data("{foo: 'bar'}");
    CHECK(v == json5::data::object_type({{"foo", "bar"}}));
}