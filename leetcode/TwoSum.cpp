vector<int> twoSum(vector<int>& nums, int target) {
    std::unordered_map<int64_t, int> tb;
    std::vector<int> ret;
    for (auto i = 0; i < nums.size(); i++) {
        const auto& num = nums[i];
        auto remains = target - num;
        auto iter = tb.find(remains);
        if (iter == tb.end()) {
            tb[num] = i;
        } else {
            ret.push_back(iter->second);
            ret.push_back(i);
            return ret;
        }
    }
    return ret;
}