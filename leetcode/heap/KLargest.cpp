#include <vector>
#include <queue>

using namespace std;

class Solution {
    template<typename T>
    using MinHeap = std::priority_queue<T, typename std::vector<T>, std::greater<T>>;
public:
    int findKthLargest(vector<int>& nums, int k) {
        MinHeap<int> h;
        for (auto i = 0; i < nums.size(); i++) {
            h.push(nums[i]);
            while (h.size() > k) {
                h.pop();
            }
        }
        return h.top();
    }
};