#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <regex>
#include <ios>
#include <type_traits>
#include <typeinfo>
#include <cstddef>
#include <stdexcept>
#include <cstring>
#include <ostream>
// from https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c
#ifndef _MSC_VER
#  if __cplusplus < 201103
#    define CONSTEXPR11_TN
#    define CONSTEXPR14_TN
#    define NOEXCEPT_TN
#  elif __cplusplus < 201402
#    define CONSTEXPR11_TN constexpr
#    define CONSTEXPR14_TN
#    define NOEXCEPT_TN noexcept
#  else
#    define CONSTEXPR11_TN constexpr
#    define CONSTEXPR14_TN constexpr
#    define NOEXCEPT_TN noexcept
#  endif
#else  // _MSC_VER
#  if _MSC_VER < 1900
#    define CONSTEXPR11_TN
#    define CONSTEXPR14_TN
#    define NOEXCEPT_TN
#  elif _MSC_VER < 2000
#    define CONSTEXPR11_TN constexpr
#    define CONSTEXPR14_TN
#    define NOEXCEPT_TN noexcept
#  else
#    define CONSTEXPR11_TN constexpr
#    define CONSTEXPR14_TN constexpr
#    define NOEXCEPT_TN noexcept
#  endif
#endif  // _MSC_VER

namespace util {
namespace {
template<typename T> void formatHelper(std::stringstream& ss, T val);

// Recursive helper implementation, puts val into the stream
// and then calls formatHelper with the remaining arguments.
template<typename T, typename ...Ts>
void formatHelper(std::stringstream& ss, T val, Ts... args) {
        ss << val;
        formatHelper(ss, args...);
}

// Base case implementation when we only have one type to
// print.
template<typename T>
void formatHelper(std::stringstream& ss, T val) {
        ss << val;
}
} /* namespace anon */

// Formats the values given to it via a std::stringstream and returns
// the resulting string, use as a shorthand for creating the
// stringstream and putting stuff into it.
template<typename T, typename ...Ts>
std::string format(T val, Ts... args) {
        std::stringstream ss;
        // force default formatting of floats
        ss << std::fixed;
        formatHelper(ss, val, args...);
        return ss.str();
}

// Remove one ' or " in the beginning and end of the
// string, e.g: "abc" => abc, and ""abc" => "abc
inline std::string stripQuotes(std::string const& s) {
        auto res = s;
        if (res.back() == '\'' || res.back() == '"') {
                res.pop_back();
        }
        if (res.front() == '\'' || res.front() == '"') {
                res.erase(0, 1);
        }
        return res;
}

// Add or remove `suffix` from the string `s`.
inline std::string addOrRemoveSuffix(std::string s, char suffix) {
        if (s.back() == suffix) {
                s.pop_back();
        } else {
                s.push_back(suffix);
        }
        return s;
}

// Also from https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c
class static_string {
        const char* const p_;
        const std::size_t sz_;

public:
        typedef const char* const_iterator;

        template <std::size_t N>
        CONSTEXPR11_TN static_string(const char(&a)[N]) NOEXCEPT_TN
                : p_(a)
                , sz_(N-1)
        {}

        CONSTEXPR11_TN static_string(const char* p, std::size_t N) NOEXCEPT_TN
                : p_(p)
                , sz_(N)
        {}

        CONSTEXPR11_TN const char* data() const NOEXCEPT_TN {return p_;}
        CONSTEXPR11_TN std::size_t size() const NOEXCEPT_TN {return sz_;}

        CONSTEXPR11_TN const_iterator begin() const NOEXCEPT_TN {return p_;}
        CONSTEXPR11_TN const_iterator end()   const NOEXCEPT_TN {return p_ + sz_;}

        CONSTEXPR11_TN char operator[](std::size_t n) const {
                return n < sz_ ? p_[n] : throw std::out_of_range("static_string");
        }
};

inline std::ostream& operator<<(std::ostream& os, static_string const& s) {
        return os.write(s.data(), s.size());
}

// This is preeeetty hacky, but we get somewhat readable type names with this until we can find something else.
template <class T> CONSTEXPR14_TN static_string type_name() {
#ifdef __clang__
        static_string p = __PRETTY_FUNCTION__;
        return static_string(p.data() + 31, p.size() - 31 - 1);
#elif defined(__GNUC__)
        static_string p = __PRETTY_FUNCTION__;
#  if __cplusplus < 201402
        return static_string(p.data() + 36, p.size() - 36 - 1);
#  else
        return static_string(p.data() + 58, p.size() - 58 - 1);
#  endif
#elif defined(_MSC_VER)
        static_string p = __FUNCSIG__;
        return static_string(p.data() + 38, p.size() - 38 - 7);
#endif
}

// is thrown by extract() when it fails to extract the value
struct ExtractionError : std::runtime_error {
        using std::runtime_error::runtime_error;
};

// try to extract the type `T` from `s` by using a
// stringstream. Throws a ExtractionError if the stream is in
// fail() after the extraction.
template<typename T>
T extract(std::string s, std::enable_if_t<std::is_default_constructible<T>::value>* = 0) {
        T t;
        std::stringstream ss{s};
        ss >> t;
        if (ss.fail()) {
                throw ExtractionError{format("Can't extract value of type `", type_name<T>(), "' from value `", s, "'")};
        }
        return t;
}
} /* namespace util */

#endif /* UTIL_H */
