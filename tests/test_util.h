#ifndef TEST_UTIL_H
#define TEST_UTIL_H

namespace test {
// Does the given value exist in the given collection?
template<typename Collection>
bool hasValue(Collection const& c, typename Collection::value_type val) {
        return std::find(c.begin(), c.end(), val) != c.end();
}
}

#endif /* TEST_UTIL_H */
