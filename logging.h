#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <mutex>
#include <memory>
#include <map>
#include <vector>

//TODO: perhaps let dbg, info etc have variadic arguments so that you
//can log any type in some sensible way?
#define LDBG(logger, msg) (*logger).dbg(__LINE__, __FILE__, msg);
#define LINFO(logger, msg) (*logger).info(__LINE__, __FILE__, msg);
#define LWARN(logger, msg) (*logger).warn(__LINE__, __FILE__, msg);
#define LPANIC(logger, msg) (*logger).panic(__LINE__, __FILE__, msg);

namespace logging {

struct Error : std::runtime_error {
        using std::runtime_error::runtime_error;
};

// Represents the destination a log will write to.
class Dest {
public:
        // Write a message to the destination
        virtual void write(std::string message) = 0;
        virtual ~Dest() {}
};

class FileDest : public Dest {
public:
        FileDest(std::string fileName);

        void write(std::string message) override;
private:
        std::fstream file{};
};

class DummyDest : public Dest {
public:
        void write(std::string message) override {}
};

class StdOutDest : public Dest {
public:
        void write(std::string message) override { std::cout << message; }
};

struct Level {
        static const Level Dbg;
        static const Level Info;
        static const Level Warn;
        static const Level Panic;

        Level(int val) : val{val} {}
        Level(Level const& other) : val{other.val} {}

        Level operator|(Level const& rhs) const {
                return Level{rhs.val | val};
        }

        Level& operator|=(Level const& rhs) {
                val |= rhs.val;
                return *this;
        }
                
        Level operator&(Level const& rhs) const {
                return Level{rhs.val & val};
        }

        Level& operator&=(Level const& rhs) {
                val &= rhs.val;
                return *this;
        }

        Level operator~() const {
                return Level{~val};
        }

        bool hasLevel(Level const& level) const {
                return (level.val & val) != 0;
        }

        void addLevel(Level const& level) {
                val |= level.val;
        }

        void removeLevel(Level const& level) {
                val = ~level.val & val;
        }
private:
        int val;
};

class Log;
using LogPtr = std::shared_ptr<Log>;

// Represents something that can do logging
// TODO: add template param for threaded or not?
class Log {
public:
        // TODO: allow use of rdbuf() to change where the ostream
        // writes instead of just having the ability of passing in the
        // stream at construction
        Log(std::string name, Level level = Level::Info | Level::Warn | Level::Panic, bool threaded = true);
        Log(std::string name, std::unique_ptr<Dest>&& dest, Level level = Level::Info | Level::Warn | Level::Panic, bool threaded = true);
        virtual ~Log() {}

        // Decide to where logging should happen
        void setDest(std::unique_ptr<Dest>&& newDest);

        // Set a new debugging level, is a bitmask of the various
        // levels.
        void setLevel(Level level);

        // Create a sublogger that will use the same destination as
        // this one but can be disabled/enabled if the need arises. A
        // new logger is enabled by default.
        LogPtr sub(std::string const& name);

        // Enable or disable subloggers that have been created with
        // sub(), if the given name doesn't exist false is returned,
        // otherwise the return value is true.
        bool disable(std::string const& name);
        bool enable(std::string const& name);
        // Same as the above, just for a path of subloggers, e.g. if
        // the sublogger has a sublogger in turn which you want to
        // disable.
        bool disable(std::vector<std::string> path);
        bool enable(std::vector<std::string> path);

        // Is the given logger name currently enabled?
        bool enabled(std::string const& name);

        // TODO: think this through and possibly add support?
        // Start a transaction, the log contents will come in the
        // order you call them, making sure that other threads using
        // this logger at the same time won't interfere.
        // int begin();
        // Call this when you're done doing logging calls and want
        // them to be commited to the log as a whole, use the id that
        // begin() gave back.
        // void commit(int id);

        // This is useful when you're trying to debug something
        virtual void dbg(int line, std::string file, std::string msg);

        // Inform about some event, nothing that too interesting, just
        // the usual operation of the system.
        virtual void info(int line, std::string file, std::string msg);

        // Warn about some kind of condition that probably isn't nice
        // but won't matter much to use being able to continue
        // running.
        virtual void warn(int line, std::string file, std::string msg);

        // Something that really should not happen happened, do
        // exit(EXIT_FAILURE)
        virtual void panic(int line, std::string file, std::string msg);

        // Change the format string to newFormat, special tokens in
        // the formatstring are:
        //  * {file} - name of the file where the log function was called
        //  * {line} - line of the file where the log function was called
        //  * {name} - name of the logger
        //  * {severity} - severity of the logged message, e.g. warning
        //  * {msg} - the actual log message
        void setFormat(std::string newFormat);

        // We do this to be able to work with our macros in a somewhat
        // sensible way. It is not the nicest
        Log& operator*() { return *this; }
protected:
        virtual void dbgInternal(int line, std::string file, std::string name, std::string msg);
        virtual void infoInternal(int line, std::string file, std::string name, std::string msg);
        virtual void warnInternal(int line, std::string file, std::string name, std::string msg);
        virtual void panicInternal(int line, std::string file, std::string name, std::string msg);

        // TODO: This is kind of nasty, but i don't know how to work around
        // it, we could do: https://stackoverflow.com/questions/6310720/declare-a-member-function-of-a-forward-declared-class-as-friend
        // but that seems like overkill.
        friend class SubLog;
        
        // Name of this log
        std::string name;
private:
        std::string levelToString(Level level);
        // Does actual logging
        void doLogInternal(Level level, int line, std::string file, std::string name, std::string msg);
        // Formats a log message according to `format`.
        std::string formatMsg(std::string level, std::string fullName, int line, std::string file, std::string message);
        // Replaces a `needl` in the `haystack` with `replacement`.
        std::string replace(std::string needle, std::string haystack, std::string replacement);
        
        // Locks/unlocks the mutex if threaded is true.
        void lock();
        void unlock();
        
        // Change the state of a logger to be either disabled or
        // enabled depending on `val`.
        bool changeState(std::string const& name, bool val);
        bool changeState(std::vector<std::string> path, bool val);
        
        // Where we want to save our log
        std::unique_ptr<Dest> dest;
        Level level;
        // Decides how the log should be formatted, see setFormat()
        std::string format;
        // Should we ensure that logging calls are serialized?
        bool threaded;
        // Used to serialize the logging calls
        std::mutex mutex;
        // Keeps track of our direct children, so that we can
        // enable/disable them at will
        std::map<std::string, LogPtr> subLoggers;
        // Stores if a certain logger is enabled or disabled
        std::map<std::string, bool> subLoggerStates;

public:
        static Log& root();
};

// Represents a logger that has a parent logger to which it sends the
// logging requests
class SubLog : public Log {
public:
        SubLog(std::string name, Log& parent);
        
        void dbg(int line, std::string file, std::string msg) override;
        void info(int line, std::string file, std::string msg) override;
        void warn(int line, std::string file, std::string msg) override;
        void panic(int line, std::string file, std::string msg) override;
protected:
        void dbgInternal(int line, std::string file, std::string name, std::string msg) override;
        void infoInternal(int line, std::string file, std::string name, std::string msg) override;
        void warnInternal(int line, std::string file, std::string name, std::string msg) override;
        void panicInternal(int line, std::string file, std::string name, std::string msg) override;
private:
        Log& parent;
};

} /* namespace logging */

#endif /* LOGGER_H */
