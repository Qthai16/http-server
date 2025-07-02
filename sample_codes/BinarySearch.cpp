#include <gtest/gtest.h>
#include <vector>
#include <algorithm>

// #include "libs/RandomUtils.h"

int lower_bound(const std::vector<int> &a, int target) {
    if (a.empty()) return 0;
    auto l = a[0];
    auto h = a.size();
    int m;
    while (l < h) {
        m = (l + h) / 2;
        if (a[m] < target) {
            l = m + 1;
        } else {
            h = m;
        }
    }
    return l;
}

bool binary_search(const std::vector<int> &a, int target) {
    auto i = lower_bound(a, target);
    return (i < a.size() && a[i] == target);
}

// TEST(binary_search, tc1) {
//     std::vector<int> a;
//     for (auto i = 0; i < 100; i++) {
//         a.push_back(libs::random_num(0, 1000));
//     }
//     std::sort(a.begin(), a.end());
//     for (auto i = 0; i < 50; i++) {
//         auto v = libs::random_num(0, 1000);
//         auto d = std::lower_bound(a.begin(), a.end(), v) - a.begin();
//         ASSERT_EQ(d, lower_bound(a, v));
//     }
// }
