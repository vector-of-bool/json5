#pragma once

#include <map>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace json5 {

template <typename Traits>
class basic_data {
public:
    using traits_type  = Traits;
    using string_type  = typename traits_type::string_type;
    using number_type  = typename traits_type::number_type;
    using boolean_type = typename traits_type::boolean_type;
    using null_type    = typename traits_type::null_type;
    using array_type   = typename traits_type::template make_array_type<basic_data>;
    using object_type  = typename traits_type::template make_object_type<basic_data>;
    using mapping_type = object_type;

    using variant_type = std::variant<null_type,  //
                                      string_type,
                                      number_type,
                                      boolean_type,
                                      array_type,
                                      object_type>;

private:
    variant_type _var;

public:
    constexpr basic_data() = default;

    template <typename Arg, typename = std::enable_if_t<std::is_convertible_v<Arg, variant_type>>>
    constexpr basic_data(Arg&& arg)
        : _var(std::forward<Arg>(arg)) {}

    constexpr basic_data(int i)
        : _var(number_type(i)) {}

    constexpr basic_data(const char* string)
        : _var(string_type(string)) {}

    template <typename T>
    constexpr bool holds_alternative() const noexcept {
        return std::holds_alternative<T>(_var);
    }

    constexpr bool is_null() const noexcept { return holds_alternative<null_type>(); }
    constexpr bool is_string() const noexcept { return holds_alternative<string_type>(); }
    constexpr bool is_number() const noexcept { return holds_alternative<number_type>(); }
    constexpr bool is_boolean() const noexcept { return holds_alternative<boolean_type>(); }
    constexpr bool is_array() const noexcept { return holds_alternative<array_type>(); }
    constexpr bool is_object() const noexcept { return holds_alternative<object_type>(); }

    template <typename T>
    constexpr T& as() {
        return std::get<T>(_var);
    }

    template <typename T>
    constexpr const T& as() const {
        return std::get<T>(_var);
    }

    constexpr null_type&    as_null() { return as<null_type>(); }
    constexpr string_type&  as_string() { return as<string_type>(); }
    constexpr number_type&  as_number() { return as<number_type>(); }
    constexpr boolean_type& as_boolean() { return as<boolean_type>(); }
    constexpr array_type&   as_array() { return as<array_type>(); }
    constexpr object_type&  as_object() { return as<object_type>(); }

    constexpr const null_type&    as_null() const { return as<null_type>(); }
    constexpr const string_type&  as_string() const { return as<string_type>(); }
    constexpr const number_type&  as_number() const { return as<number_type>(); }
    constexpr const boolean_type& as_boolean() const { return as<boolean_type>(); }
    constexpr const array_type&   as_array() const { return as<array_type>(); }
    constexpr const object_type&  as_object() const { return as<object_type>(); }

    constexpr friend bool operator==(const basic_data& lhs, const basic_data& rhs) noexcept {
        return lhs._var == rhs._var;
    }

    constexpr friend bool operator!=(const basic_data& lhs, const basic_data& rhs) noexcept {
        return lhs._var != rhs._var;
    }

    constexpr friend bool operator<(const basic_data& lhs, const basic_data& rhs) noexcept {
        return lhs._var < rhs._var;
    }

    constexpr friend bool operator<=(const basic_data& lhs, const basic_data& rhs) noexcept {
        return lhs._var <= rhs._var;
    }

    constexpr friend bool operator>(const basic_data& lhs, const basic_data& rhs) noexcept {
        return lhs._var > rhs._var;
    }

    constexpr friend bool operator>=(const basic_data& lhs, const basic_data& rhs) noexcept {
        return lhs._var >= rhs._var;
    }
};

struct default_data_traits {
    using string_type  = std::string;
    using number_type  = double;
    using boolean_type = bool;
    using null_type    = decltype(nullptr);

    template <typename T>
    using make_array_type = std::vector<T>;

    template <typename T>
    using make_object_type = std::map<string_type, T>;
};

using data = basic_data<default_data_traits>;

}  // namespace json5
