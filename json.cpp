#include "json.h"
#include "util.h"

#include "js0n.h"

#include <iostream>
#include <fstream>
#include <iterator>
#include <tuple>

namespace json {

JsonStructured::JsonStructured(std::string const& fileName) : fileName{fileName} {
        std::fstream f{fileName};
        if (!f.is_open()) {
                throw Error{util::format("Can't open file `", fileName, "' for reading.")};
        }
        fromStream(f);
}

JsonStructured::JsonStructured(std::istream& stream) {
        fromStream(stream);
}

void JsonStructured::fromStream(std::istream& stream) {
        json = std::string(std::istreambuf_iterator<char>(stream), {});
        if (stream.bad()) {
                throw Error{util::format("Unknown error while reading config file `", fileName, "'.")};
        }
}

std::vector<std::string> JsonStructured::lookupArray(std::initializer_list<std::string> path, bool quoteStrings /* = false */) const {
        char const* ptr;
        size_t len;
        if (path.size() == 0) {
                ptr = json.c_str();
                len = json.size();
        } else {
                std::tie(ptr, len) = lookupPath(path);
        }

        // (ptr, len) now holds info on the array we want to access
        int i{0};
        size_t vlen;
        std::vector<std::string> result;
        while (true) {
                auto currPtr = js0n(nullptr, i, ptr, len, &vlen);
                if (!currPtr) {
                        // we got to the last element in the array
                        break;
                }
                if (vlen == static_cast<size_t>(-1)) {
                        throw ParseError{util::format("Can't read array index `", i, "'")};
                }
                if (*(currPtr - 1) == '"') {
                        if (json.c_str() + json.size() > ptr) {
                                currPtr -= 1;
                                vlen += 2;
                        } else {
                                // This might imply other things than the
                                // message says, but this'll have to do for
                                // now
                                // TODO: More descriptive error message?
                                ptrdiff_t byteIndex{json.c_str() + json.size() - ptr};
                                throw ParseError{util::format("Found beginning of string with no end, at byte `", byteIndex, "'")};
                        }
                }
                ++i;
                result.push_back(std::string(currPtr, vlen));
        }
        return result;
}

std::string JsonStructured::lookupString(std::initializer_list<std::string> path) const {
        if (path.size() == 0) {
                throw ParseError{"Path for lookup is empty."};
        }
        char const* ptr;
        size_t len;
        std::tie(ptr, len) = lookupPath(path);
        return std::string{ptr, len};
}

std::tuple<char const*, size_t> JsonStructured::lookupPath(std::initializer_list<std::string> path) const {
        char const* ptr;
        size_t len;
        auto it = path.begin();
        std::tie(ptr, len) = lookupHelper(*it, json);
        ++it;
        if (it != path.end()) {
                for (; it != path.end(); ++it) {
                        std::tie(ptr, len) = lookupHelper(*it, std::string(ptr, len));
                }
        }
        return std::make_tuple(ptr, len);
}

std::tuple<char const*, size_t> JsonStructured::lookupHelper(std::string const& key, std::string const& data) const {
        size_t vlen{};
        char const* ptr = js0n(key.c_str(), key.length(),
                               data.c_str(), data.size(),
                               &vlen);
        if (!ptr) {
                throw FieldNotFoundError{util::format("Can't find key `", key, "'")};
        }
        if (vlen == static_cast<size_t>(-1)) {
                throw ParseError{util::format("Can't parse json while looking for key `", key, "'")};
        }
        assert(vlen < data.size());
        return std::make_tuple(ptr, vlen);
}

} /* namespace json */
