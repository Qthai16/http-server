class NumArray {
public:
    NumArray(vector<int>& nums) {
        long long sum = 0;
        presum.resize(nums.size() + 1);
        presum[0] = 0;
        for (auto i = 0; i < nums.size(); i++) {
            sum += nums[i];
            presum[i+1] = sum;
        }
    }
    
    int sumRange(int left, int right) {
        // presum[r] - presum[l] = a[r] + a[r-1] + ... + a[l+1]
        // a[l] = presum[l] - presum[l-1]
        // --> answer = presum[r] - presum[l-1]
        // notes: l and r are array index, presum index is array index + 1
        // formula: a[l] + a[l+1] + ... + a[r] =  
        return presum[right+1] - presum[left];
    }
private:
    std::vector<long long> presum;
};