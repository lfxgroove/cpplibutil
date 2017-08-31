#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "logging.h"

int main(int argc, char** argv) {
        doctest::Context ctx;

        // Turn off logging
        logging::Log::root().setDest(std::make_unique<logging::DummyDest>());
        
        ctx.applyCommandLine(argc, argv);
        int res = ctx.run();
        return res;
}
