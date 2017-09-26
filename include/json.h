#ifndef JSON_H
#define JSON_H

#include <string>
#include <initializer_list>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <chrono>

#include "util.h"

namespace json {

struct Error : std::runtime_error {
        using std::runtime_error::runtime_error;
};

struct ParseError : Error {
        using Error::Error;
};

struct FieldNotFoundError : Error {
        using Error::Error;
};

// Helpers for lookup
template<typename T>
struct JsonStructuredLookup {
        static const bool enabled = false;
        // Convert `value` to something of type `T`. Should return
        // false if the conversion failed and true otherwise. The
        // value of `ret` is not guaranteed to not have changed even
        // if the conversion failed.
        static bool doConversion(std::string const& value, T& ret);
};

template<>
struct JsonStructuredLookup<int> {
        static const bool enabled = true;
        static bool doConversion(std::string const& value, int& ret) {
                std::stringstream ss;
                ss << value;
                ss >> ret;
                return !!ss;
        }
};

template<>
struct JsonStructuredLookup<std::string> {
        static const bool enabled = true;
        static bool doConversion(std::string const& value, std::string& ret) {
                ret = value;
                return true;
        }
};

template<>
struct JsonStructuredLookup<bool> {
        static const bool enabled = true;
        static bool doConversion(std::string const& value, bool& ret) {
                if (value == "0" || value == "false" || value == "FALSE") {
                        ret = false;
                } else {
                        ret = true;
                }
                return true;
        }
};

template<>
struct JsonStructuredLookup<std::chrono::milliseconds> {
        static const bool enabled = true;
        static bool doConversion(std::string const& val, std::chrono::milliseconds& ret) {
                double secs;
                try {
                        secs = util::extract<double>(val);
                } catch(util::ExtractionError const& e) {
                        throw json::ParseError{e.what()};
                }
                std::chrono::duration<double, std::ratio<1, 1>> seconds{secs};
                ret = std::chrono::duration_cast<std::chrono::milliseconds>(seconds);
                return true;
        }
};

// class JsonUnstructured;

// JSON parser. Works by being given the path that one wants to
// examine, for example:
//
// ```
//  {
//   "a": {
//      "b": 10
//     }
//  }
// ```
//
// to get the value 10 we would give the path ("a", "b") to
// lookupString() or lookup(). Use this class when you have structured
// data and you know what paths that will exist, if that is not known
// you can use JsonUnstructured which will parse the JSON into a
// json::Object.
class JsonStructured {
        // These two need to know about Str, so we friend them as we don't wanna expose Str to
        // everyone.
        friend class JsonUnstructured;
        friend class Parser;
        
        // A string class that allows us to have pointers to substrings of this string. Use this
        // with care as there is no reference counting for the pointers into the string, so your
        // pointers will become invalid whenever the dtor for this class runs. This is a non-sane
        // alternative to a string_view, but we don't have c++17 :(
        // TODO: Change to c++17 or use refcounting/shared pointers
        class Str {
        public:
                Str(Str const&) = delete;
                Str(Str&&) = default;
                Str& operator=(Str const&) = delete;
                Str& operator=(Str&& other) {
                        // We can't assume that the pointers remain intact through a move, therefore
                        // we need to do some bookkeeping
                        std::swap(other.str, str);
                        other.ptr = other.str.c_str();
                        other.len = other.str.size();
                        ptr = str.c_str();
                        len = str.size();
                        return *this;
                }
                Str(char const *ptr, size_t len) : str{""}, ptr{ptr}, len{len} {}
                Str(std::string inStr) : str{inStr}, ptr{str.c_str()}, len{str.size()} {}
                Str() : str{""}, ptr{nullptr}, len{0} {}
                ~Str() {};

                using const_iterator = char const *;
                const_iterator begin() const { return c_str(); }
                const_iterator end() const { return c_str() + size(); }
                
                char const *c_str() const { return ptr; }
                size_t size() const { return len; }
        private:
                // this string is never used, but needs to be kept around so that we can keep the
                // references to the inside of the string alive.
                std::string str;
                char const *ptr;
                size_t len;
        };
public:
        // Create with the contents in a file named `fileName`
        JsonStructured(std::string const& fileName);
        // Create with contents available in the istream `stream`
        JsonStructured(std::istream& stream);

        // Lookup a string following the given path of objects.
        std::string lookupString(std::initializer_list<std::string> path) const;

        // Lookup a array of strings following the given path of objects, if quoteStrings is true
        // strings will come back quoted
        std::vector<std::string> lookupArray(std::initializer_list<std::string> path, bool quoteStrings = false) const;

        // Try to lookup the given path in a json file, true is returned if the value could be
        // parsed as the type T. Specializations currently exist for int, bool, string, a generic
        // JSON objects. New ones can be added by specializing the struct JsonStructuredLookup. For
        // an example, see JsonStructuredLookup<int>. Throws ParseError, and any other exceptions
        // thrown by custom parsing helpers
        template<typename T>
        bool lookup(std::initializer_list<std::string> path, T& t, typename std::enable_if<JsonStructuredLookup<T>::enabled>::type* = 0) const {
                auto s = lookupString(path);
                return JsonStructuredLookup<T>::doConversion(s, t);
        }

        // Lookups the given path, and try to parse the array as the given type `T`. If one of the
        // conversions fail false is returned, however you won't know which value that wasn't
        // convertable. Throws ParseError, and any other exceptions thrown by custom parsing helpers
        template<typename T>
        bool lookup(std::initializer_list<std::string> path, std::vector<T>& vec, typename std::enable_if<JsonStructuredLookup<T>::enabled>::type* = 0) const {
                auto values = lookupArray(path);
                bool retVal = true;
                for (auto const& val : values) {
                        T t;
                        if (!JsonStructuredLookup<T>::doConversion(val, t)) {
                                retVal = false;
                        }
                        vec.push_back(t);
                }
                return retVal;
        }

        // Move forward in the json until we have traversed the given path, return a pointer to the
        // content at the end of the path. The retrieved pointer should not be freed and will live
        // as long as the 
        std::tuple<char const*, size_t> lookupPath(std::initializer_list<std::string> path) const;
        
private:
        // Retrieve the json string this parser is working on. The string is not guaranteed to
        // contain valid json.
        Str const& data() const { return json; }
        
        // helper to make lookup()/lookupArray() more pleasant to write.
        std::tuple<char const*, size_t> lookupHelper(std::string const& key, Str const& data) const;

        // initialize this config reader from the given stream.
        void fromStream(std::istream& stream);
        
        // file contents (json only) that we will work with, no
        // validation is done that the values in here are actual json.
        // std::string json{};
        Str json{};
        // Used to give better diagnostics if we fail while reading
        // from the file. Is unknown if the stream constructor has
        // been used.
        std::string fileName{"unknown"};
};
} /* namespace json */

#endif /* JSON_H */
