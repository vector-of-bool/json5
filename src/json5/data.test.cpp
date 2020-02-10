#include <json5/data.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Basic types") {
    json5::data def;
    CHECK(def.is_null());

    json5::data str = "string";
    CHECK(str.is_string());

    def = "some string";
    CHECK(def == "some string");
    CHECK(def != nullptr);
}