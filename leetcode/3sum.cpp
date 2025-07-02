class Solution {
public:
    vector<vector<int>> threeSum(vector<int>& nums) {
        // sort array, use two pointers with one index start from [0, size - 2)
        std::sort(nums.begin(), nums.end());
        int j = 0, k = nums.size();
        int64_t sum = 0;
        using Triplet = std::tuple<int, int, int>;
        std::set<Triplet> ret;  
        // vector<vector<int>> ret;
        for (auto i = 0; i < nums.size() - 2; i++) {
            if (i > 0 && nums[i] == nums[i-1]) { // skip same value
                continue;
            }
            j = i + 1;
            k = nums.size() - 1;
            while (j < k) {
                sum = nums[j] + nums[k] + nums[i];
                if (sum == 0) {
                    // printf("%d, %d, %d\n", nums[i], nums[j], nums[k]);
                    ret.insert({nums[i], nums[j], nums[k]});
                    j += 1;
                } else if (sum < 0) {
                    j += 1;
                } else k -= 1;
            }
        }
        vector<vector<int>> retv;
        for (const auto& ele : ret) {
            retv.push_back({get<0>(ele), get<1>(ele), get<2>(ele)});
        }
        return retv;
    }
};