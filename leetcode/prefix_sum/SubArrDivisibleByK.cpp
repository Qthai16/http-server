// https://leetcode.com/problems/subarray-sums-divisible-by-k/description/

// https://stackoverflow.com/questions/7594508/why-does-the-modulo-operator-result-in-negative-values
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
using namespace std;

class Solution {
    int nChoose2(int n) {
        return n*(n-1)/2;
    }
public:
    int subarraysDivByK(vector<int> &nums, int k) {
        if (nums.empty()) return 0;
        if (nums.size() == 1) return nums[0] % k == 0 ? 1 : 0;
        int ret = 0;
        vector<int> presum(nums.size() + 1, 0);
        int64_t sum = 0;
        for (auto i = 0; i < nums.size(); i++) {
            sum += nums[i];
            presum[i + 1] = sum;
        }
        unordered_map<int, int> tb; // mod: freq
        for (auto i = 1; i < presum.size(); i++) {
            auto mod = presum[i]%k;
            if (mod < 0) tb[k + mod] += 1;
            else tb[mod] += 1;
        }
        for (const auto& p : tb) {
            auto remain = p.first;
            auto freq = p.second;
            if (freq <= 1 && remain != 0) continue;
            if (remain == 0) {
                if (freq == 1) ret += 1;
                else ret += freq + nChoose2(freq);
            } else {
                ret += nChoose2(freq);
            }
        }
        return ret;
    }
};