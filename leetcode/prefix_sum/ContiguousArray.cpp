// https://leetcode.com/problems/contiguous-array/
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <tuple>
using namespace std;

class Solution {
public:
    int findMaxLength(vector<int>& nums) {
        vector<int> presum(nums.size() + 1, 0);
        for (auto i = 0; i < nums.size(); i++) {
            presum[i+1] = presum[i] + (nums[i] == 0 ? -1 : 1);
        }
        int maxlen = 0;
        unordered_map<int, pair<int, int>> tb; // presum: first index, last index
        for (auto i = 0; i < presum.size(); i++) {
            if (tb.count(presum[i]) == 0) tb[presum[i]] = {i, -1};
            else tb[presum[i]].second = i;
            if (tb[presum[i]].second != -1)
                maxlen = max(maxlen, tb[presum[i]].second - tb[presum[i]].first);
        }
        return maxlen;
    }
};
