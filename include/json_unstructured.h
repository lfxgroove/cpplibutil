#ifndef JSON_UNSTRUCTURED_H
#define JSON_UNSTRUCTURED_H

#include <string>
#include <map>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <boost/variant.hpp>
#include <boost/blank.hpp>

#include "json.h"
#include "util.h"

namespace json {
class JsonStructured;

struct NullType {};

struct ObjectError : std::runtime_error::runtime_error {
        using std::runtime_error::runtime_error;
};

struct BadTypeError : ObjectError {
        using ObjectError::ObjectError;
};

namespace {
        template<typename T>
        struct TypeConversion : boost::static_visitor<T> {
                T operator()(T val) const {
                        return val;
                }

                template<typename U>
                T operator()(U val) const {
                        throw BadTypeError{util::format("Trying to convert Object to `", util::type_name<T>(), "' when it is of type `", util::type_name<U>(), "'")};
                }
        };

        template<typename T>
        struct IsType : boost::static_visitor<bool> {
                bool operator()(T const& val) const { return true; }

                template<typename U>
                bool operator()(U const& val) const { return false; }
        };
}

struct Property;

using Path = std::vector<std::string>;
        
// Aliases to ease use of into()
class Object;
using Str = std::string;
using Int = std::int64_t;
using Double = double;
using Bool = bool;
using Arr = std::vector<Object>;
using Obj = std::map<std::string, Object>;
using Null = NullType;

// A general Object that holds some kind of json data, the datatypes
// are roughly from json.org. The difference is that both a Double and
// Int type exists, while json.org only has a Number type. Two objects
// compare equal if they are of the same underlying type and have the
// same contents.
struct Object {

        using ValueType = boost::variant<boost::blank, Null, Str, Int, Double, Bool, Arr, Obj>;

        Object(ValueType value) : value{value} {}
        // We add this one so that the Str works as it should,
        // otherwise a char* make us call the Int constructor
        Object(char const* value) : Object(std::string(value)) {}
        // Implicit constructors to make life easier
        Object(Str const& value) : value{value} {}
        Object(std::int32_t const& value) : value{static_cast<Int>(value)} {}
        Object(Int const& value) : value{value} {}
        Object(Double const& value) : value{value} {}
        Object(Bool const& value) : value{value} {}
        Object(Arr const& value) : value{value} {}
        Object(Obj const& value) : value{value} {}
        Object() : value{boost::blank()} {}

        static Object parse(std::string const& s);

        // Converts a vector<T> to a vector<Object> if there is a
        // fitting constructor for Object that takes T as a parameter.
        // TODO: create version for map<std::string, ..> as well?
        template<typename T>
        static std::vector<Object> convert(std::vector<T> const& v, typename std::enable_if<std::is_convertible<T, Object>::value>::type* = 0) {
                std::vector<Object> res;
                std::transform(v.begin(), v.end(), std::back_inserter(res),
                               [](T const& t) {
                                       return Object(t);
                               });
                return res;
        }
        
        // Is this Object empty/blank? I.e. is there no value stored
        // in it currently?
        bool blank() const;

        // serialize us down to a JSON representation, depth decides
        // how much we should indent what we return, i.e. how many
        // spaces that should be prepended to the serialized content
        // when the content is of type std::map<...>
        std::string prettyPrint(int depth = 4) const;

        // serialize the contents to a string.
        std::string serialize() const;

        // Add a property to something of type Obj, returns true if
        // the property got added, false if the property already
        // existed. If it already exists the old value is removed. If
        // parts of the path along the way do not exist, they will be
        // created.
        bool addProperty(Path path, Property prop);
        // Add a property to this Object, it must be of type Obj
        // for this to work. If the property already existed true is
        // returned and it is replaced, otherwise false is returned.
        bool addProperty(Property prop);

        // Push a value of some kind onto an array, if the path does
        // not lead to an array an error is thrown.
        void push(Path path, Object value);
        // Push a value onto ourselves, must be of type Arr, otherwise
        // an exception is thrown
        void push(Object value);
        
        // Retrieve the given path and convert it into type `T`, if
        // conversion is not possible an exception of type
        // BadTypeError is thrown.
        template<typename T>
        T const& get(Path path) const {
                if (path.empty()) {
                        // We're at the end of the path and are the
                        // object we're searching for.
                        return into<T const&>();
                } else {
                        // Lookup the object inside ourselves, then
                        // dispatch away to the object we found
                        Path rest{path.begin() + 1, path.end()};
                        return get(*path.begin()).get<T>(rest);
                }
        }

        // TODO: how can we avoid doing the same thing in both these
        // functions?
        template<typename T>
        T& get(Path path) {
                if (path.empty()) {
                        // We're at the end of the path and are the
                        // object we're searching for.
                        return into<T&>();
                } else {
                        // Lookup the object inside ourselves, then
                        // dispatch away to the object we found
                        Path rest{path.begin() + 1, path.end()};
                        return get(*path.begin()).get<T>(rest);
                }
        }

        // TODO: how can we avoid doing the same thing in both these
        // functions? Get the given path as the type `T`, if parts of
        // the path do not exist they are created on the way.
        template<typename T>
        T& getOrInsert(Path path) {
                if (path.empty()) {
                        // We're at the end of the path and are the
                        // object we're searching for.
                        return into<T&>();
                } else {
                        // Lookup the object inside ourselves, then
                        // dispatch away to the object we found
                        Path rest{path.begin() + 1, path.end()};
                        return getOrInsert(*path.begin()).getOrInsert<T>(rest);
                }
        }

        // Retrieve the keys for this object if it is of type Obj. E.g:
        // 
        // ```
        // { "test": [1, 2, 3], "cat": { "dog": "woof"} }
        // ```
        //
        // Would have the keys "test" and "cat". An error is thrown if
        // this Object isn't of type Obj.
        std::vector<std::string> keys() const;
        
        // Retrieve a named part from something with type Obj
        Object const& get(std::string const& name) const;
        Object& get(std::string const& name);
        // Retrieve a named part from something with type Obj, if it
        // doesn't exist, we insert it and return it.
        Object const& getOrInsert(std::string const& name) const;
        Object& getOrInsert(std::string const& name);

        // Retrieve a certain index from something with type Arr
        Object const& get(int index) const;
        Object& get(int index);
        
        // Check how long this object is given that it is Arr. Throws
        // if the object isn't Arr.
        int length() const;

        // Is this object of the actual type T? See the available
        // aliases for possible types to pass in.
        template<typename T>
        bool is() const {
                return boost::apply_visitor(IsType<T>(), value);
        }

        // "Unpack" this json object into whatever `T` is, throws
        // BadTypeError if `T` isn't the type this object actually
        // is.
        // TODO: return T& here instead of T
        template<typename T>
        T into() const {
                return boost::apply_visitor(TypeConversion<T>(), value);
        }

        template<typename T>
        T into() {
                return boost::apply_visitor(TypeConversion<T>(), value);
        }

        // Only works when util::extract<T>() exists
        template<typename T>
        std::map<std::string, T> into(typename std::enable_if<!std::is_same<T, Object>::value>::type) {
                Obj o = boost::apply_visitor(TypeConversion<std::map<std::string, T>>(), value);
                // Obj o = into<Obj>();
                std::map<std::string, T> res;
                // for (auto it = o.begin(); it != o.end(); ++it) {
                for (auto const& it : o) {
                        res[it.first] = util::extract<T>(it.second);
                }
                return res;
        }

        bool operator==(Object const& rhs) const;
        bool operator!=(Object const& rhs) const;
private:
        template<typename T>
        bool compareVariant(Object const& lhs, Object const& rhs) const {
                if (lhs.is<T>() && rhs.is<T>()) {
                        return lhs.into<T>() == rhs.into<T>();
                } else {
                        return false;
                }
        }
        
        template<typename ...Ts>
        bool isEqual(Object const& lhs, Object const& rhs) const {
                bool results[sizeof...(Ts)] = { (compareVariant<Ts>(lhs, rhs))... };
                return std::any_of(&results[0], &results[sizeof...(Ts)], [](bool b) { return b; });
        }
        
        ValueType value;
};

struct Property {
        std::string name;
        Object value;
};

// Parses JSON data and creates a Object that can be used to
// extract the information programmatically.
struct Parser {
        Parser() = default;

        // Parse the string `s` that we were created with and return
        // the Object that it represents. Almost no guarantees are
        // given that the parser will error out on invalid data, and
        // very little information about what went wrong is available.
        static Object parse(std::string const& s);

        Object parseArr(std::string s);
        Object parseString(std::string const& s);
        Object parseHelper(std::string s);
        Object parseInt(std::string const& s);
        Object parseDouble(std::string const& s);
        Object parseObj(std::string s);
        Object parseBool(std::string const& s);

        // Does `s` contain the beginning of an object?
        bool obj(std::string const& s) const;
        // Does `s` contain the beginning of an array?
        bool arr(std::string const& s) const;
        // Does `s` contain the beginning of an int?
        bool i(std::string const& s) const;
        // Does `s` contain the beginning of a double?
        bool dbl(std::string const& s) const;
        // Is `s` a bool value? Only 'true' and 'false' are
        // considered, no typecasting from e.g. 0 -> 'false' happens.
        bool b(std::string const& s) const;
        // Is `s` the null value?
        bool null(std::string const& s) const;
        // Is `s` the beginning of a string?
        bool str(std::string const& s) const;
private:
        // return point where s stops containing a integer, gives -1
        // when we can't find anything resembling a int.
        size_t iPos(std::string const& s) const;

        // Lookup a value in the object and make sure to keep
        // quotation marks so that strings can be identified
        std::tuple<char const*, size_t> lookupValue(JsonStructured& json, std::string const& key) const;

        std::string findKey(JsonStructured::Str const& s) const;
        std::string findKey(JsonStructured::Str::const_iterator start, JsonStructured::Str::const_iterator end) const;

        bool find(std::string const& s, char needle) const {
                for (char c : s) {
                        if (c == needle) {
                                return true;
                        }
                        if (c != ' ' && c != '\t') {
                                return false;
                        }
                }
                return false;
        }
        
        std::string json{};
};

template<>
struct JsonStructuredLookup<Object> {
        static const bool enabled = true;
        static bool doConversion(std::string const& val, json::Object& ret) {
                ret = Parser::parse(val);
                return true;
        }
};

} /* namespace json */

#endif /* JSON_UNSTRUCTURED_H */
