# Intro

This is a small collection of somewhat useful C++ classes. The following things currently exist:

* JsonStructured/JsonUnstructured
* Config
* Log
* Util

# Building

This uses [meson](http://mesonbuild.com) for buildling, one way of building it is like this:

```
    mkdir obj
    meson obj
    cd obj
    ninja
```

# Library contents

## JSON

JsonStructured/JsonUnstructured is a small JSON parser based on the js0n library found
[here](https://github.com/quartzjer/js0n). The structured JSON is more lightweight and a thin
wrapper around js0n. It is probably best to use if you know the exact structure that the data is
going to have. Example usage of JsonStructured:

```c++
    #include "json.h"

    /* ... */

    std::string jsonData{R"<({
    "obj": {
        "value": true,
        "an_arr": [true, false],
    },
    "another_value": false,
})<"};
    std::stringstream ss{jsonData};
    // It is also possible to give a fileName to the constructor
    json::JsonStructured json{ss};
    bool boolValue{};
    if (json.lookup({"obj", "value"}, boolValue)) {
        std::cout << "Extracted value from json: " << boolValue << std::endl;
    } else {
        std::cout << "Couldn't extract value from json" << std::endl;
    }
```

JsonUnstructured on the other hand can parse a JSON object and return the type `json::Object` which
is a representation of the JSON that was parsed. This object can then be queried for information in
different ways. JsonUnstructured is more useful if you don't know all the elements that will exist
or if they might be of different data types depending on the context. Example usage:

```c++
    std::string json{R"<({
    "obj": {
        "value": true,
        "an_arr": [true, false],
    },
    "another_value": false,
})<"};

    json::Object result = json::Parser::parse(json);
    // This will throw if the value isn't of type bool.
    std::cout << "obj->value = " << result.get<json::Bool>({"obj", "value"}) << std::endl;
    try {
        json::Arr const& arr = result.get<json::Arr>({"obj", "an_arr"});
        std::cout << "Size of array: " << arr.size() << ", should be 2" << std::endl;
        if (arr[0].is<json::Bool>()) {
            std::cout << "The first element has value: " << arr[0].into<json::Bool>() << std::endl;
        }
    } catch(json::BadTypeError const& e) {
        std:::cout << "Could not get array from the object" << std::endl;
    }
```

## Config
Config is a very small wrapper around a JSON object, providing some convenience functions to easier
make casts as the keys in a config file are usually known.

## Log
A fairly simple and small logging class, you can either access `logging::Log::root()` to get access
to a root logger from which you can then create sub loggers for different tasks. This would be done
as follows:

```c++
    auto logger = logging::Log::root().sub("myCategory");
    LWARN(logger, "This is a warning");
    LINFO(logger, "This is information");
    LWARN(logging::Log::root(), "This is a warning from the root category");
    // Only show the message in the logs, see setFormat() documentation for possible replacements
    logging::Log::root().setFormat("{msg}\n");
```

If you don't want to use the root logger one can create a root logger themselves:

```c++
    logging::Log log{"myRootLoggerName"};
    auto sub = log.sub("AnotherLogger");
    LWARN(log, "warning from my own root");
```

The destination of the logs is determined by calling `setDest()`, implementing a new destination is
done by subclassing `logging::Dest` and implementing the `write()` function in there. Only the root
logger can change the destination and format of the log. By default logging is done with
`logging::StdOutDest`, meaning that all the data is written to stdout.

## Util
Small collection of utility things, a few string handling functions and some template magic.

### util::type_name<T>()
Use this to get a more sensible name than `typeid(T).name()` would give you under certain
circumstances, this is given by an answer on stackoverflow. Use as follows:

```c++
    template<class T>
    class MyClass {};
    std::cout << util::type_name<MyClass<std::string>>() << std::endl;
```

### util::format()
Use to format a string without having to use a stringstream explicitly. E.g:

```c++
    std::string formatted = util::format("hello", 1, 2, 3, 10.0, false);
```

is equivalent to:

```c++
    std::stringstream ss;
    ss << std::fixed;
    ss << "hello" << 1 << 2 << 3 << 10.0 << false;
    std::string formatted = ss.str();
```

### util::extract()
Does the opposite of `util::format()`, throws an exception if the value couldn't be converted via
the stringstream.

```c++
    int i;
    util::extract("10", i);
    assert(i == 10);
```