#ifndef JSON_H
#define JSON_H

#include <string>
#include <initializer_list>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <chrono>

#include "util.h"
#include "json_unstructured.h"

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

template<>
struct JsonStructuredLookup<Object> {
        static const bool enabled = true;
        static bool doConversion(std::string const& val, json::Object& ret) {
                ret = Parser::parse(val);
                return true;
        }
};

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
public:
        // Create with the contents in a file named `fileName`
        JsonStructured(std::string const& fileName);
        // Create with contents available in the istream `stream`
        JsonStructured(std::istream& stream);

        // Lookup a string following the given path of objects.
        std::string lookupString(std::initializer_list<std::string> path) const;

        // Lookup a array of strings following the given path of
        // objects, if quoteStrings is true strings will come back
        // quoted
        std::vector<std::string> lookupArray(std::initializer_list<std::string> path, bool quoteStrings = false) const;

        // Try to lookup the given path in a json file, true is
        // returned if the value could be parsed as the type
        // T. Specializations currently exist for int, bool, string, a
        // generic JSON objects. New ones can be added by specializing
        // the struct JsonStructuredLookup. For an example, see
        // JsonStructuredLookup<int>. Throws ParseError, and any other
        // exceptions thrown by custom parsing helpers
        template<typename T>
        bool lookup(std::initializer_list<std::string> path, T& t, typename std::enable_if<JsonStructuredLookup<T>::enabled>::type* = 0) const {
                auto s = lookupString(path);
                return JsonStructuredLookup<T>::doConversion(s, t);
        }

        // Lookups the given path, and try to parse the array as the
        // given type `T`. If one of the conversions fail false is
        // returned, however you won't know which value that wasn't
        // convertable. Throws ParseError, and any other exceptions
        // thrown by custom parsing helpers
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

        // move forward in the json until we have traversed the given
        // path, return a pointer to the content at that path.
        // TODO: document me better
        std::tuple<char const*, size_t> lookupPath(std::initializer_list<std::string> path) const;

        // Retrieve a reference to the underlying json string. The
        // string is not guaranteed to contain valid json.
        std::string const& data() const { return json; }
        
private:
        // helper to make lookup()/lookupArray() more pleasant to write.
        std::tuple<char const*, size_t> lookupHelper(std::string const& key, std::string const& data) const;

        // initialize this config reader from the given stream.
        void fromStream(std::istream& stream);
        
        // file contents (json only) that we will work with, no
        // validation is done that the values in here are actual json.
        std::string json{};
        // Used to give better diagnostics if we fail while reading
        // from the file. Is unknown if the stream constructor has
        // been used.
        std::string fileName{"unknown"};
};
} /* namespace json */

#endif /* JSON_H */
