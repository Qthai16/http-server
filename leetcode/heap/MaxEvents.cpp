#include <queue>
#include <algorithm>

// https://leetcode.com/problems/maximum-number-of-events-that-can-be-attended/description/

using namespace std;

class Solution {
    template<typename T>
    using MinHeap = std::priority_queue<T, typename std::vector<T>, std::greater<T>>;
    template<typename T>
    using MaxHeap = std::priority_queue<T>;
public:
    int maxEvents(vector<vector<int>> &events) {
        sort(events.begin(), events.end());
        const auto n = events.size();
        MinHeap<int> h;
        int ind = 0;
        int ret = 0;
        int day = events[0][0];
        while (ret < n) {
            // start: events[ind][0]
            // end: events[ind][1]
            // start <= i <= end
            // check available events, push end day
            while (ind < events.size() && events[ind][0] <= day) {
                h.push(events[ind][1]);
                ind++;
            }
            // pop expired events: i > end day
            while (!h.empty() && day > h.top()) {
                h.pop();
            }
            if (!h.empty()) {
                // select the top of the heap (event that end earliest)
                h.pop();
                ret++;
            }
            day++;
            if (ind >= events.size() && h.empty()) break;
        }
        return ret;
    }
};