#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <cstdlib>

using namespace std;

class Solution {
public:
    int maximumLength(vector<int>& nums) {
        if (nums.size() == 0) return 0;
        int evenCnt = 0, oddCnt = 0, mixCnt = 0;
        auto preEven = nums[0] % 2 == 0;
        if (preEven) evenCnt++;
        else oddCnt++;
        for (auto i = 1; i < nums.size(); i++) {
            auto isEven = nums[i]%2 == 0;
            if (isEven) {
                evenCnt++;
            } else oddCnt++;
            if (isEven^preEven) {
                preEven = isEven;
                mixCnt++;
            }
        }
        // printf("odd: %d, even: %d, mix: %d, drop: %d\n",
        //     oddCnt, evenCnt, mixCnt, dropCnt);
        return max(oddCnt, max(evenCnt, mixCnt + 1));
    }
};