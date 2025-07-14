#include <queue>
#include <unordered_map>

using namespace std;

class Solution {
    template<typename T>
    using MinHeap = std::priority_queue<T, typename std::vector<T>, std::greater<T>>;

public:
    vector<int> topKFrequent(vector<int> &nums, int k) {
        MinHeap<std::pair<int, int>> h; // frequent: value
        unordered_map<int, int> tb;
        for (auto i = 0; i < nums.size(); i++) {
            tb[nums[i]] += 1;
        }
        for (const auto& p : tb) {
            h.push({p.second, p.first});
            if (h.size() > k) {
                h.pop();
            }
        }
        vector<int> ret;
        ret.resize(h.size());
        auto i = 0;
        while (!h.empty()) {
           ret[i++]  = h.top().second;
           h.pop();
        }
        return ret;
    }
};