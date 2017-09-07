#include "logging.h"

#include "util.h"

#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <cassert>

namespace logging {

const Level Level::Dbg = Level(1u);
const Level Level::Info = Level(1u << 1);
const Level Level::Warn = Level(1u << 2);
const Level Level::Panic = Level(1u << 3);

//TODO: Should we just coarsely lock every function or do we want to
//device something smart? Probably doesn't matter too much if we do
//the coarse thing?
LogPtr Log::sub(std::string const& name) {
        lock();
        subLoggers[name] = std::make_shared<SubLog>(name, *this);
        subLoggerStates[name] = true;
        unlock();
        return subLoggers[name];
}

bool Log::changeState(std::string const& name, bool val) {
        auto it = subLoggerStates.find(name);
        bool found = it != subLoggerStates.end();
        if (found) {
                it->second = false;
        }
        return found;
}

bool Log::changeState(std::vector<std::string> path, bool val) {
        assert(path.size() > 0);
        if (path.size() == 1) {
                return changeState(path[0], val);
        } else if (path.size() == 2) {
                return changeState(path[1], val);
        } else {
                auto it = subLoggers.find(path.at(1));
                if (it != subLoggers.end()) {
                        auto v = std::vector<std::string>(path.begin() + 1, path.end());
                        return it->second->changeState(v, val);
                }
        }
        return false;
}

bool Log::disable(std::vector<std::string> path) {
        return changeState(path, false);
}

bool Log::enable(std::vector<std::string> path) {
        return changeState(path, true);
}

bool Log::disable(std::string const& name) {
        return changeState(name, false);
}

bool Log::enable(std::string const& name) {
        return changeState(name, true);
}

SubLog::SubLog(std::string name, Log& parent)
        : Log{name}, parent(parent) {}

void SubLog::dbg(int line, std::string file, std::string msg) {
        parent.dbgInternal(line, file, name, msg);
}

void SubLog::info(int line, std::string file, std::string msg) {
        parent.infoInternal(line, file, name, msg);
}

void SubLog::warn(int line, std::string file, std::string msg) {
        parent.warnInternal(line, file, name, msg);
}

void SubLog::panic(int line, std::string file, std::string msg) {
        parent.panicInternal(line, file, name, msg);       
}

void SubLog::dbgInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                parent.dbgInternal(line, file, this->name + "/" + name, msg);
        }
}

void SubLog::infoInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                parent.infoInternal(line, file, this->name + "/" + name, msg);
        }
}

void SubLog::warnInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                parent.warnInternal(line, file, this->name + "/" + name, msg);
        }
}

void SubLog::panicInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                parent.panicInternal(line, file, this->name + "/" + name, msg);
        }
}

bool Log::enabled(std::string const& name) {
        auto it = subLoggerStates.find(name);
        if (it != subLoggerStates.end()) {
                return it->second;
        }
        return false;
}

Log::Log(std::string name, std::unique_ptr<Dest>&& dest, Level level /* = Level::Info | Level::Warn | Level::Panic */, bool threaded /* = true */)
        : name{name}, dest{std::move(dest)}, level{level},
          format{"[{severity} ({name})]: {msg}\n"} {}

Log::Log(std::string name, Level level /* = Level::Info | Level::Warn | Level::Panic */, bool threaded /* = true */)
        : Log{name, util::make_unique<StdOutDest>(), level} {}

void Log::setFormat(std::string newFormat) {
        lock();
        format = newFormat;
        unlock();
}

void Log::setLevel(Level newLevel) {
        lock();
        level = newLevel;
        unlock();
}

void Log::dbgInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                doLogInternal(Level::Dbg, line, file, this->name + "/" + name, msg);
        }
}

void Log::infoInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                doLogInternal(Level::Info, line, file, this->name + "/" + name, msg);
        }
}

void Log::warnInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                doLogInternal(Level::Warn, line, file, this->name + "/" + name, msg);
        }
}

void Log::panicInternal(int line, std::string file, std::string name, std::string msg) {
        if (enabled(name)) {
                doLogInternal(Level::Panic, line, file, this->name + "/" + name, msg);
        }
}

void Log::doLogInternal(Level level, int line, std::string file, std::string name, std::string msg) {
        lock();
        if (dest) {
                dest->write(formatMsg(levelToString(level), name, line, file, msg));
        } else {
                throw Error{"There is no destination available for logging"};
        }
        unlock();
}

std::string Log::levelToString(Level level) {
        if (level.hasLevel(Level::Dbg))   { return "DEBUG  "; }
        if (level.hasLevel(Level::Info))  { return "INFO   "; }
        if (level.hasLevel(Level::Warn))  { return "WARNING"; }
        if (level.hasLevel(Level::Panic)) { return "PANIC  "; }
        throw std::runtime_error{"Unreachable code in Log::levelToString()"};
}

std::string Log::replace(std::string needle, std::string haystack, std::string replacement) {
        std::string res{};
        auto pos = haystack.find(needle);
        if (pos == std::string::npos) {
                return haystack;
        }
        res = haystack.substr(0, pos);
        res += replacement;
        res += haystack.substr(pos + needle.length());
        return res;
}

std::string Log::formatMsg(std::string level, std::string fullName, int line, std::string file, std::string message) {
        std::string res{format};
        res = replace("{file}", res, file);
        res = replace("{line}", res, util::format(line));
        res = replace("{name}", res, fullName);
        res = replace("{severity}", res, level);
        res = replace("{msg}", res, message);
        return res;
}

void Log::dbg(int line, std::string file, std::string msg) {
        doLogInternal(Level::Dbg, line, file, name, msg);
}

void Log::info(int line, std::string file, std::string msg) {
        doLogInternal(Level::Info, line, file, name, msg);
}

void Log::warn(int line, std::string file, std::string msg) {
        doLogInternal(Level::Warn, line, file, name, msg);
}

void Log::panic(int line, std::string file, std::string msg) {
        doLogInternal(Level::Panic, line, file, name, msg);
}

void Log::setDest(std::unique_ptr<Dest>&& newDest) {
        lock();
        dest = std::move(newDest);
        unlock();
}

void Log::lock() {
        if (threaded) {
                mutex.lock();
        }
}

void Log::unlock() {
        if (threaded) {
                mutex.unlock();
        }
}

FileDest::FileDest(std::string fileName) {
        file.open(fileName, std::ios::out | std::ios::app);
        if (!file.is_open()) {
                throw Error{util::format("Can't open `", fileName, "' for writing")};
        }
}

void FileDest::write(std::string message) {
        file << message;
}

// If i've understood https://stackoverflow.com/a/11667596 correctly
// this should be thread safe
Log& Log::root() {
        static Log root{"root"};
        return root;
}

} /* namespace logging */
