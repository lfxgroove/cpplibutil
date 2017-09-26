#include "json_unstructured.h"

#include "json.h"

namespace json {

std::string Parser::findKey(std::string const& s) const {
        return findKey(s.begin(), s.end());
}

std::string Parser::findKey(std::string::const_iterator start, std::string::const_iterator end) const {
        std::string::const_iterator resStart = end, resEnd = end;
        for (auto it = start; it != end; ++it) {
                // Means we've reached the end of a key: value pair,
                // the key didn't start with a " and therefore we
                // can't continue
                if (*it == ':' && resStart == end) {
                        throw ParseError{util::format("Could not find a key before the value started, looked in `", std::string{start, it}, "' before encountering the beginning of a value. Keys must be quoted.")};
                }
                // Means we've reached the end of the object and we can't really do much more.
                if (*it == '}' && resStart == end) {
                        return "";
                }
                if (*it == '"' && resStart != end && *(it - 1) != '\\') {
                        resEnd = it;
                        break;
                }
                if (*it == '"' && resStart == end) {
                        resStart = it + 1;
                }
        }
        return std::string{resStart, resEnd};
}

Object Parser::parseArr(std::string s) {
        std::stringstream ss{s};
        JsonStructured json{ss};
        std::vector<std::string> values = json.lookupArray({}, true);
        std::vector<Object> result;
        for (auto const& val : values) {
                result.push_back(parseHelper(val));
        }
        return Object{result};
}

Object Parser::parseObj(std::string s) {
        Obj obj;
        std::stringstream ss{s};
        JsonStructured json{ss};
        
        //find the first key in this object
        std::string key = findKey(s);
        if (key == "") {
                throw ParseError{util::format("Can't find a key for object in input: `", s, "'. Keys must have quotation marks around them.")};
        }

        // ptr will be a pointer into data() owned by JsonStructured
        char const* ptr;
        size_t len;
        
        std::tie(ptr, len) = lookupValue(json, key);

        obj[key] = parseHelper(std::string{ptr, len});
        
        // find the rest of the keys in this object
        while (true) {
                // jump past what we just handled
                ptr += len;
                std::string const& data = json.data();
                key = findKey(data.begin() + (ptr - data.c_str()), data.end());
                if (key == "") {
                        break;
                }
                std::tie(ptr, len) = lookupValue(json, key);
                obj[key] = parseHelper(std::string{ptr, len});
        }
        return Object{obj};
}

Object Parser::parseString(std::string const& s) {
        return Object{util::stripQuotes(s)};
}

Object Parser::parseInt(std::string const& s) {
        // this can throw
        // perhaps 64bit instead?
        int i = util::extract<int>(s);
        return Object{i};
}

Object Parser::parseDouble(std::string const& s) {
        // this can throw
        double d = util::extract<double>(s);
        return Object{d};
}

Object Parser::parseBool(std::string const& s) {
        if (s == "true") {
                return Object{true};
        } else {
                return Object{false};
        }
}

Object Parser::parseHelper(std::string s) {
        if (arr(s)) {
                return parseArr(s);
        }
        if (obj(s)) {
                return parseObj(s);
        }
        if (dbl(s)) {
                return parseDouble(s);
        }
        if (i(s)) {
                return parseInt(s);
        }
        if (b(s)) {
                return parseBool(s);
        }
        if (null(s)) {
                return Object{NullType()};
        }
        if (str(s)) {
                return parseString(s);
        }
        throw ParseError{util::format("Encountered unknown token when processing `", s, "'")};
}

size_t Parser::iPos(std::string const& s) const {
        if (s.size() < 1) {
                return static_cast<size_t>(-1);
        }
        if (s[0] != '-' && !std::isdigit(s[0])) {
                return static_cast<size_t>(-1);
        }
        for (auto it = s.begin() + 1; it != s.end(); ++it) {
                if (!std::isdigit(*it)) {
                        return it - s.begin() - 1;
                }
        }
        return s.size() - 1;
}

std::tuple<char const*, size_t> Parser::lookupValue(JsonStructured& json, std::string const& key) const {
        char const* ptr;
        size_t len;
        std::tie(ptr, len) = json.lookupPath({key});
        // Make sure that we keep the string delimiters so that we can
        // properly identify strings if that is needed.
        if (*(ptr - 1) == '"') {
                std::string const& data = json.data();
                if (data.c_str() + data.size() > ptr) {
                        ptr -= 1;
                        len += 2;
                } else {
                        // This might imply other things than the
                        // message says, but this'll have to do for
                        // now
                        // TODO: More descriptive error message?
                        ptrdiff_t byteIndex{data.c_str() + data.size() - ptr};
                        throw ParseError{util::format("Found beginning of string with no end, at byte `", byteIndex, "'")};
                }
        }
        return std::make_tuple(ptr, len);
}

bool Parser::i(std::string const& s) const {
        size_t pos = iPos(s);
        if (pos == static_cast<size_t>(-1)) {
                return false;
        }
        return pos == s.size() - 1;
}

bool Parser::dbl(std::string const& s) const {
        size_t pos = iPos(s);
        if (pos == static_cast<size_t>(-1) || pos == s.size() - 1) {
                return false;
        }
        //123.123e12
        if (s[pos + 1] == '.' && s.size() - 1 > pos + 1) {
                std::string mid = s.substr(pos + 2);
                // this covers 123.123
                size_t midPos = iPos(mid);
                if (midPos == mid.size() - 1) {
                        return true;
                }
                if (midPos == static_cast<size_t>(-1)) {
                        return false;
                }
                if (mid[midPos + 1] == 'e' || mid[midPos + 1] == 'E') {
                        std::string end = mid.substr(midPos + 2);
                        return iPos(end) == end.size() - 1;
                }
        }
        //123e12
        if ((s[pos + 1] == 'e' || s[pos + 1] == 'E') && s.size() - 1 > pos + 1) {
                std::string end = s.substr(pos + 2);
                return iPos(end) == end.size() -1;
        }
        return false;
}

bool Parser::str(std::string const& s) const {
        if (s.length() < 1) {
                return false;
        }
        if (s[0] != '"') {
                return false;
        }
        for (auto it = s.begin() + 1; it != s.end() - 1; ++it) {
                if (*it == '"' && *(it - 1) != '\\') {
                        return false;
                }
        }
        return s.back() == '"';
}

bool Parser::null(std::string const& s) const {
        return s == "null";
}

bool Parser::b(std::string const& s) const {
        return s == "true" || s == "false";
}

bool Parser::obj(std::string const& s) const {
        return find(s, '{');
}
        
bool Parser::arr(std::string const& s) const {
        return find(s, '[');
}

Object Parser::parse(std::string const& json) {
        return Parser{}.parseHelper(json);
}

namespace {
        // from https://stackoverflow.com/questions/31320857/how-to-determine-if-a-boostvariant-is-empty
        struct IsBlank : boost::static_visitor<bool> {
                bool operator()(boost::blank) const { return true; }
                        
                template<typename T>
                bool operator()(T const&) const { return false; }
        };
}

bool Object::blank() const {
        return boost::apply_visitor(IsBlank(), value);
}

bool Object::operator==(Object const& rhs) const {
        return isEqual<boost::blank, Str, Int, Double, Bool, Arr, Obj>(*this, rhs);
}

bool Object::operator!=(Object const& rhs) const {
        return !(*this == rhs);
}

bool Object::addProperty(Path path, Property prop) {
        Obj& obj = getOrInsert<Obj>(path);
        bool hadProp = obj.find(prop.name) != obj.end();
        obj[prop.name] = prop.value;
        return hadProp;
}

bool Object::addProperty(Property prop) {
        Obj& obj = into<Obj&>();
        bool hadProp = obj.find(prop.name) != obj.end();
        obj[prop.name] = prop.value;
        return hadProp;
}

void Object::push(Path path, Object value) {
        Arr& arr = get<Arr>(path);
        arr.push_back(value);
}

void Object::push(Object value) {
        into<Arr&>().push_back(value);
}

namespace {
        template<typename T, typename U>
        struct ExtractFromObj : boost::static_visitor<U> {
                std::string name{};
                ExtractFromObj(std::string const& name) :
                        name{name} {}

                U operator()(T val) const {
                        return val.at(name);
                }

                template<typename V>
                U operator()(V) const {
                        throw ObjectError{util::format("Trying to get property from object when object is of type `", util::type_name<T>(), "' and not of type `", util::type_name<U>(), "'")};
                }
        };

        template<typename T, typename U>
        struct ExtractFromObjAndAdd : boost::static_visitor<U> {
                std::string name{};
                bool create{false};
                ExtractFromObjAndAdd(std::string const& name) :
                        name{name} {}

                U operator()(T val) const {
                        if (val.find(name) == val.end()) {
                                val[name] = Obj();
                        }
                        return val.at(name);
                }

                template<typename V>
                U operator()(V) const {
                        throw ObjectError{util::format("Trying to get property from object when object is of type `", util::type_name<T>(), "' and not of type `", util::type_name<U>(), "'")};
                }
        };
}

Object& Object::getOrInsert(std::string const& name) {
        if (blank()) {
                auto val = json::Obj();
                val[name] = json::Obj();
                value = val;
        }
        return boost::apply_visitor(ExtractFromObjAndAdd<Obj&, Object&>(name), value);
}

Object const& Object::get(std::string const& name) const {
        return boost::apply_visitor(ExtractFromObj<Obj const&, Object const&>(name), value);
}

Object& Object::get(std::string const& name) {
        return boost::apply_visitor(ExtractFromObj<Obj&, Object&>(name), value);
}

namespace {
template<typename T, typename U>
struct ExtractFromArr : boost::static_visitor<U> {
        int index;
        ExtractFromArr(int index) : index{index} {}
                
        U const& operator()(T val) const {
                return val.at(index);
        }

        template<typename V>
        U const& operator()(V) const {
                throw ObjectError{util::format("Trying to get position `", index, "' from object of type `", util::type_name<V>(), "' when it should be `", util::type_name<U>(), "'")};
        }
};
} /* namespace anon */

Object const& Object::get(int index) const {
        return boost::apply_visitor(ExtractFromArr<Arr const&, Object const&>(index), value);
}

Object& Object::get(int index) {
        return boost::apply_visitor(ExtractFromArr<Arr&, Object&>(index), value);
}

namespace {
struct ExtractKeysFromObj : boost::static_visitor<std::vector<std::string>> {
        std::vector<std::string> operator()(Obj const& val) const {
                std::vector<std::string> res{};
                for (auto const& it : val) {
                        res.push_back(it.first);
                }
                return res;
        }

        template<typename T>
        std::vector<std::string> operator()(T val) const {
                throw ObjectError{util::format("Trying to get keys for object which isn't of type `json::Obj', but instead is `", util::type_name<T>(), "'")};
        }
};
} /* namespace anon */

std::vector<std::string> Object::keys() const {
        return boost::apply_visitor(ExtractKeysFromObj(), value);
}

int Object::length() const {
        return into<Arr>().size();
}

std::string Object::prettyPrint(int depth /* = 4*/) const {
        struct PrettyPrint : boost::static_visitor<std::string> {
                int depth;
                PrettyPrint(int depth) : depth{depth} {}
                        
                std::string operator()(boost::blank) const { return ""; }

                std::string operator()(Str const& val) const { return "\"" + val + "\""; }

                std::string operator()(NullType const&) const { return "null"; }

                std::string operator()(Double const& val) const {
                        return util::format(val);
                }

                std::string operator()(Int const& val) const {
                        return util::format(val);
                }

                std::string operator()(Bool const& val) const {
                        if (val) {
                                return "true";
                        } else {
                                return "false";
                        }
                }

                std::string operator()(Arr const& val) const {
                        std::stringstream ss;
                        ss << "[";
                        for (unsigned i{0}; i < val.size(); ++i) {
                                ss << val[i].prettyPrint();
                                if (i < val.size() - 1) {
                                        ss << ", ";
                                }
                        }
                        ss << "]";
                        return ss.str();
                }
                
                std::string operator()(Obj const& val) const {
                        std::stringstream ss;
                        ss << "{\n";
                        for (auto const& it : val) {
                                for (int i = 0; i < depth; ++i) {
                                        ss << ' ';
                                }
                                ss << "\"" << it.first <<"\": " << it.second.prettyPrint(depth + 2) << ",\n";
                        }
                        for (int i = 0; i < depth - 2; ++i) {
                                ss << ' ';
                        }
                        ss << "}";
                        return ss.str();
                }
        };
        return boost::apply_visitor(PrettyPrint(depth), value);
}

std::string Object::serialize() const {
        struct Serialize : boost::static_visitor<std::string> {
                std::string operator()(boost::blank) const { return ""; }

                std::string operator()(Str const& val) const { return "\"" + val + "\""; }

                std::string operator()(NullType const&) const { return "null"; }

                std::string operator()(Double const& val) const {
                        std::string ret;
                        std::stringstream ss;
                        ss << val;
                        ss >> ret;
                        return ret;
                }

                std::string operator()(Int const& val) const {
                        std::string ret;
                        std::stringstream ss;
                        ss << val;
                        ss >> ret;
                        return ret;
                }

                std::string operator()(Bool const& val) const {
                        if (val) {
                                return "true";
                        } else {
                                return "false";
                        }
                }

                std::string operator()(Arr const& val) const {
                        std::stringstream ss;
                        ss << "[";
                        for (unsigned i{0}; i < val.size(); ++i) {
                                ss << val[i].serialize();
                                if (i < val.size() - 1) {
                                        ss << ",";
                                }
                        }
                        ss << "]";
                        return ss.str();
                }
                
                std::string operator()(Obj const& val) const {
                        std::stringstream ss;
                        ss << "{";
                        for (auto it = val.begin(); it != val.end(); ++it) {
                                ss << "\"" << it->first << "\":" << it->second.serialize();
                                auto fwd = it;
                                fwd++;
                                if (fwd != val.end()) {
                                        ss << ",";
                                }
                        }
                        ss << "}";
                        return ss.str();
                }
        };
        return boost::apply_visitor(Serialize(), value);
}

} /* namespace json */
