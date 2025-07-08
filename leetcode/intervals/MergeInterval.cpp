// Given an array of intervals where intervals[i] = [starti, endi], merge all overlapping intervals, and return an array of the non-overlapping intervals that cover all the intervals in the input.

// Example 1:

// Input: intervals = [[1,3],[2,6],[8,10],[15,18]]
// Output: [[1,6],[8,10],[15,18]]
// Explanation: Since intervals [1,3] and [2,6] overlap, merge them into [1,6].
// Example 2:

// Input: intervals = [[1,4],[4,5]]
// Output: [[1,5]]
// Explanation: Intervals [1,4] and [4,5] are considered overlapping.


// Constraints:

// 1 <= intervals.length <= 104
// intervals[i].length == 2
// 0 <= starti <= endi <= 104

#include <vector>
#include <algorithm>
#include <set>
#include <cstdio>
#include <cstdlib>

using namespace std;


class Solution {
public:
    vector<vector<int>> merge(vector<vector<int>> &intervals) {
        std::vector<std::pair<int, int>> sorted;
        for (const auto &v: intervals) {
            sorted.push_back(std::make_pair(v[0], v[1]));
        }
        std::sort(sorted.begin(), sorted.end());
        vector<vector<int>> ret;
        for (auto it = sorted.begin(); it != sorted.end();) {
            auto next = std::next(it, 1);
            if (next == sorted.end()) {
                ret.push_back(std::vector<int>{it->first, it->second});
                break;
            }
            if (it->second >= next->first) {// merge it
                // printf("merge: [%d, %d] vs [%d, %d]\n", it->first, it->second, next->first, next->second);
                next->first = it->first;
                next->second = std::max(next->second, it->second);
                it = next;
            } else {
                ret.push_back(std::vector<int>{it->first, it->second});
                it++;
            }
        }
        return ret;
    }
};
