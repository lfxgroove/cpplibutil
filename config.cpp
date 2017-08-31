#include "config.h"

#include <fstream>

#include "util.h"

Config::Config(std::string const& fileName)  {
        std::fstream f{fileName};
        if (!f.is_open()) {
                throw ConfigError{util::format("Can't open configuration file `", fileName, "'")};
        }
        // TODO: doing this in bulk would be better, find how, also,
        // presize the string from the file size.
        std::string contents;
        char ch;
        while (f.get(ch)) {
                contents.push_back(ch);
        }
        // this can throw
        cfg = json::Parser::parse(contents);
}

Config::Config(json::Object const& obj) {
        cfg = obj;
}

json::Object const& Config::toJson() const {
        return cfg;
}

Config::Arr const& Config::arr(Path const& p) const {
	return cfg.get<json::Arr>(p);
}

Config::Obj const& Config::obj(Path const& p) const {
	return cfg.get<json::Obj>(p);
}

Config::Str const& Config::str(Path const& p) const {
	return cfg.get<json::Str>(p);
}

Config::Bool Config::b(Path const& p) const {
	return cfg.get<json::Bool>(p);
}

Config::Int Config::i(Path const& p) const {
	return cfg.get<json::Int>(p);
}

Config::Double Config::dbl(Path const& p) const {
	return cfg.get<json::Double>(p);
}



