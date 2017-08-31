#include "doctest.h"
#include "logging.h"

class StringDest : public logging::Dest {
public:
        static std::string contents;
        void write(std::string msg) {
                contents += msg;
        }

        static void reset() { contents = ""; }
};

std::string StringDest::contents;

TEST_CASE("basic logging works") {
        StringDest::reset();
        logging::Log l{"root", std::make_unique<StringDest>()};

        SUBCASE("messages are logged") {
                l.setFormat("{msg}");
                LINFO(l, "test");
                CHECK(StringDest::contents == "test");
        }
        
        SUBCASE("severity can be logged") {
                l.setFormat("{severity}");
                LINFO(l, "test");
                CHECK(StringDest::contents.substr(0, 4) == "INFO");
        }

        SUBCASE("subloggers work") {
                l.setFormat("{name}");
                auto sub = l.sub("sub");

                SUBCASE("the right logger name is used") {
                        LINFO(sub, "sub");
                        CHECK(StringDest::contents == "root/sub");
                }
                
                SUBCASE("subloggers can be disabled") {
                        l.disable("sub");
                        LINFO(sub, "sub");
                        CHECK(StringDest::contents.empty());
                }
        }
}
