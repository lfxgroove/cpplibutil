#ifndef CONFIG_H
#define CONFIG_H

#include <stdexcept>
#include "json.h"

struct ConfigError : std::runtime_error::runtime_error {
        using std::runtime_error::runtime_error;
};

// Use to read config files that are in JSON format, a simple wrapper
// around JsonUnstructured.
class Config {
public:
        using Path = json::Path;
        using Arr = json::Arr;
        using Obj = json::Obj;
        using Str = json::Str;
        using Bool = json::Bool;
        using Int = json::Int;
        using Double = json::Double;
        
        Config(std::string const& fileName);
        Config(json::Object const& obj);
        Config() = default;
        
        Arr const& arr(Path const& p) const;
        Obj const& obj(Path const& p) const;
        Str const& str(Path const& p) const;
        Bool b(Path const& p) const;
        Int i(Path const& p) const;
        Double dbl(Path const& p) const;

        json::Object const& toJson() const;

        // Add a property to this configuration, if the key, value
        // pair didn't exist before, true is returned, otherwise false
        // is returned and the old value is overwritten.
        template<typename T>
        bool addProperty(Path const& path, std::string const& key, T const& value, std::enable_if_t<std::is_convertible<T, json::Object>::value>* = 0) {
                json::Property prop{key, json::Object{value}};
                return cfg.addProperty(path, prop);
        }
        
private:
        json::Object cfg;
};

#endif /* CONFIG_H */
