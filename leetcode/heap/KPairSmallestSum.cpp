#include <vector>
#include <queue>
#include <cstdlib>

// https://leetcode.com/problems/find-k-pairs-with-smallest-sums

using namespace std;

class Solution {
    template<typename T>
    using MinHeap = std::priority_queue<T, typename std::vector<T>, std::greater<T>>;

public:
    vector<vector<int>> kSmallestPairs(vector<int> &nums1, vector<int> &nums2, int k) {
        MinHeap<pair<int, int>> heap; // sum: nums2 index
        int i = 0, j = 0, sum = 0;
        for (const auto& v : nums1) {
            heap.push({v + nums2[0], 0});
        }
        vector<vector<int>> ret;
        while (k > 0 && !heap.empty()) {
            auto v = heap.top();
            auto pos = v.second;
            auto sum = v.first;
            ret.push_back({sum - nums2[pos], nums2[pos]});
            heap.pop();
            k--;
            if (pos + 1 < nums2.size())
                heap.push({sum - nums2[pos] + nums2[pos + 1], pos + 1});
        }
        return ret;
    }
};