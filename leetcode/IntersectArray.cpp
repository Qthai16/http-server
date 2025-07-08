#include <vector>
#include <unordered_set>
#include <unordered_map>
using namespace std;

vector<int> intersection(vector<int> &nums1, vector<int> &nums2) {
    std::unordered_set<int> ret;
    std::unordered_map<int, int> lk;
    for (auto i = 0, j = 0; i < nums1.size() || j < nums2.size(); i++, j++) {
        if (i < nums1.size()) {
            auto num = lk[nums1[i]];
            lk[nums1[i]] = (num <= 0 ? num + 1 : num);
            if (lk[nums1[i]] == 0) ret.insert(nums1[i]);
        }
        if (j < nums2.size()) {
            auto num = lk[nums2[i]];
            lk[nums2[i]] = (num >= 0 ? num - 1 : num);
            if (lk[nums2[i]] == 0) ret.insert(nums2[i]);
        }
    }
    return {ret.begin(), ret.end()};
}