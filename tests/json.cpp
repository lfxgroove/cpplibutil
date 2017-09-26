#include "doctest.h"

#include "json.h"
#include "test_util.h"

#include <iostream>
#include <sstream>

TEST_CASE("parsing different values works") {
        // super simple for now
        std::stringstream json{R"<<<(
{
    "bc": {
        "port": 26000,
        "addr": "ff01::1",
        "enable": "false",
    },
}
)<<<"};

        json::JsonStructured c{json};

        SUBCASE("reading a integer") {
                int a;
                CHECK(c.lookup({"bc", "port"}, a));
                // c.lookup({"bc"}, a);
                // c.lookup({"ab", "hej"}, a);
                CHECK(a == 26000);
                CHECK_FALSE(c.lookup({"bc", "addr"}, a));
                CHECK_FALSE(c.lookup({"bc", "enable"}, a));
        }

        SUBCASE("reading a string") {
                std::string s;
                CHECK(c.lookup({"bc", "addr"}, s));
                CHECK(s == "ff01::1");
                CHECK(c.lookup({"bc", "port"}, s));
                CHECK(s == "26000");
        }

        SUBCASE("reading a bool") {
                bool b;
                CHECK(c.lookup({"bc", "enable"}, b));
                CHECK_FALSE(b);
                CHECK(c.lookup({"bc", "port"}, b));
                CHECK(b);
                CHECK(c.lookup({"bc", "addr"}, b));
                CHECK(b);
        }

        SUBCASE("reading the same bool twice") {
                bool b;
                CHECK(c.lookup({"bc", "enable"}, b));
                CHECK_FALSE(b);
                CHECK(c.lookup({"bc", "enable"}, b));
                CHECK_FALSE(b);                
        }
}

TEST_CASE("differentiating types when parsing json") {
        json::Parser parser;

        SUBCASE("identifying a double") {
                CHECK_FALSE(parser.dbl("123"));
                CHECK_FALSE(parser.dbl("123."));
                CHECK_FALSE(parser.dbl("1."));
                CHECK_FALSE(parser.dbl("."));
                CHECK_FALSE(parser.dbl("123e"));
                CHECK_FALSE(parser.dbl("laskdj"));
                CHECK(parser.dbl("10.1"));
                CHECK(parser.dbl("123.1"));
                CHECK(parser.dbl("123.12"));
                CHECK(parser.dbl("123e1"));
                CHECK(parser.dbl("123e12"));
                CHECK(parser.dbl("123.1e1"));
                CHECK(parser.dbl("123.12e12"));
        }
}

TEST_CASE("creating json::Objects that are correct") {
        std::string json{R"<<<({
    "test": {
        "nest": {
           "value": 10,
           "array": [1, 2, 3],
        },
    },
    "addr": "ff01::1",
    "enable": false,
})<<<"};

        json::Object result = json::Parser::parse(json);

        SUBCASE("data is extracted correctly") {
                CHECK(result.get<json::Int>({"test", "nest", "value"}) == 10);
                CHECK(result.get<json::Str>({"addr"}) == "ff01::1");
                CHECK(result.get<json::Bool>({"enable"}) == false);
                CHECK_NOTHROW(result.get<json::Obj>({"test"}));
                json::Arr const& arr = result.get<json::Arr>({"test", "nest", "array"});
                CHECK(arr.size() == 3);
                for (unsigned i = 0; i < arr.size(); ++i) {
                        CHECK(arr[i].is<json::Int>());
                }
        }

        SUBCASE("data is serialized correctly") {
                std::string expectedSerialization{R"<({"addr":"ff01::1","enable":false,"test":{"nest":{"array":[1,2,3],"value":10}}})<"};
                CHECK(result.serialize() == expectedSerialization);
        }
}

TEST_CASE("arrays in objects are properly parsed") {
        std::string json{R"<({
    "arg": ["hund", "mjau", 12],
})<"};
        json::Object result = json::Parser::parse(json);
        json::Arr& arr = result.get<json::Arr>({"arg"});
        CHECK_NOTHROW(result.get<json::Arr>({"arg"}));
        CHECK_THROWS_AS(result.get<json::Arr>({"arg", "test"}), json::ObjectError const&);
        REQUIRE(arr.size() == 3);
        CHECK(arr[0].is<json::Str>());
        CHECK(arr[0].into<json::Str>() == "hund");
        CHECK(arr[1].is<json::Str>());
        CHECK(arr[1].into<json::Str>() == "mjau");
        CHECK(arr[2].is<json::Int>());
        CHECK(arr[2].into<json::Int>() == 12);
}

TEST_CASE("parsing double values works") {
        SUBCASE("parsing 1e3") {
                std::string json{"1e3"};
                json::Object result = json::Parser::parse(json);

                REQUIRE(result.is<json::Double>());
                CHECK(result.into<json::Double>() == doctest::Approx(1000));
        }

        SUBCASE("parsing 1.2e3") {
                std::string json{"1.2e3"};
                json::Object result = json::Parser::parse(json);

                REQUIRE(result.is<json::Double>());
                CHECK(result.into<json::Double>() == doctest::Approx(1200));
        }
}

TEST_CASE("parsing bool values works") {
        std::string json{R"<({
    "obj": {
        "value": true,
        "an_arr": [true, false],
    },
    "another_value": false,
})<"};

        json::Object result = json::Parser::parse(json);
        CHECK(result.get<json::Bool>({"obj", "value"}));
        CHECK_FALSE(result.get<json::Bool>({"another_value"}));
        json::Arr const& arr = result.get<json::Arr>({"obj", "an_arr"});
        REQUIRE(arr.size() == 2);
        CHECK(arr[0].into<json::Bool>());
        CHECK_FALSE(arr[1].into<json::Bool>());
}

TEST_CASE("unquoted keys do not parse") {
        std::string json{R"<({
    true: "hello",
})<"};

        CHECK_THROWS_AS(json::Parser::parse(json), json::ParseError const&);
}

TEST_CASE("both ref and const-ref as return from get() works") {
        std::string json{R"<({
    "arg": ["hund", "mjau", 12],
})<"};
        json::Object result = json::Parser::parse(json);
        // We mostly want this to compile sensibly, and to not kill
        // the program when run.
        json::Arr& a = result.get<json::Arr>({"arg"});
        CHECK(a.size() == 3);
        json::Arr const& b = result.get<json::Arr>({"arg"});
        CHECK(b.size() == 3);
}

TEST_CASE("getOrInsert works") {
        json::Object obj;
        json::Obj& res = obj.getOrInsert<json::Obj>({"a", "path"});
        res["test"] = 10;
        CHECK(obj.get<json::Int>({"a", "path", "test"}) == 10);
}

TEST_CASE("addProperty works") {
        json::Object obj;
        obj.addProperty({"a", "path"}, {"test", 10});
        CHECK(obj.get<json::Int>({"a", "path", "test"}) == 10);
}

TEST_CASE("getting the keys for an object works") {
        std::string json{R"<({
    "arg": ["hund", "mjau", 12],
    "two": 14,
    "three": "value",
})<"};
        json::Object result = json::Parser::parse(json);
        auto keys = result.keys();
        CHECK(keys.size() == 3);
        CHECK(test::hasValue(keys, "arg"));
        CHECK(test::hasValue(keys, "two"));
        CHECK(test::hasValue(keys, "three"));
}

TEST_CASE("pushing to arrays works") {
        std::string json{R"<(["hund", "mjau", 12])<"};
        json::Object result = json::Parser::parse(json);
        json::Arr& arr = result.into<decltype(arr)>();
        result.push("hello");
        REQUIRE(arr.size() == 4);
        CHECK(arr[3].is<json::Str>());
        CHECK(arr[3].into<json::Str>() == "hello");
        result.push(true);
        CHECK(arr[4].is<json::Bool>());
        CHECK(arr[4].into<json::Bool>());
        CHECK(result.get(1).into<json::Str>() == "mjau");
}

TEST_CASE("into<T> for std::map<std::string, T> works as well as other overloads") {
        std::string json{R"<({"mjau": 3, "dog": 2})<"};
        json::Object result = json::Parser::parse(json);
        json::Obj obj = result.into<json::Obj>();
        CHECK(obj["mjau"].into<json::Int>() == 3);
}

TEST_CASE("parsing empty strings throws a ParseError") {
        std::string json{""};
        CHECK_THROWS_AS(json::Parser::parse(json), json::ParseError const&);
}
