class Solution {
public:
    int removeElement(vector<int>& nums, int val) {
        // using two pointer, if reach index where nums[i] == val, increase next pointer
        int i = 0, j = 0, cnt = 0;
        while (i < nums.size()) {
            if (nums[i] != val) {
                i++;
                if (j == i - 1) j++;
                cnt++;
                continue;
            }
            while(j < nums.size() && nums[j] == val) {
                j++;
            }
            if (j >= nums.size()) {
                // no element to swap
                break;
            }
            cnt++;
            auto tmp = nums[i];
            nums[i] = nums[j];
            nums[j] = tmp;
            i += 1;
            // j += 1;
        };
        return cnt;
    }
};