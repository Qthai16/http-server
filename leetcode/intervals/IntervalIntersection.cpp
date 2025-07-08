#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>

using namespace std;

// You are given two lists of closed intervals, firstList and secondList, where firstList[i] = [starti, endi] and secondList[j] = [startj, endj].
// Each list of intervals is pairwise disjoint and in sorted order.
// Return the intersection of these two interval lists.
// A closed interval [a, b] (with a <= b) denotes the set of real numbers x with a <= x <= b.
// The intersection of two closed intervals is a set of real numbers that are either empty or represented as a closed interval. For example, the intersection of [1, 3] and [2, 4] is [2, 3].
 

// Example 1:
// Input: firstList = [[0,2],[5,10],[13,23],[24,25]], secondList = [[1,5],[8,12],[15,24],[25,26]]
// Output: [[1,2],[5,5],[8,10],[15,23],[24,24],[25,25]]

// Example 2:
// Input: firstList = [[1,3],[5,9]], secondList = []
// Output: []

// Input
// firstList = [[3,5],[9,20]]
// secondList = [[4,5],[7,10],[11,12],[14,15],[16,20]]

// Use Testcase
// Output
// [[4,5],[9,10],[11,20]]
// Expected
// [[4,5],[9,10],[11,12],[14,15],[16,20]]

class Solution {
public:
    vector<vector<int>> intervalIntersection(vector<vector<int>>& firstList, vector<vector<int>>& secondList) {
        using Intv = std::pair<int, int>;
        using InitList = std::initializer_list<int>;
        std::vector<Intv> sorted;
        auto maxSize = std::max(firstList.size(), secondList.size());
        for (auto i = 0; i < maxSize; i++) {
            if (i < firstList.size())
                sorted.push_back({firstList[i][0], firstList[i][1]});
            if (i < secondList.size())
                sorted.push_back({secondList[i][0], secondList[i][1]});
        }
        std::sort(sorted.begin(), sorted.end());
        vector<vector<int>> ret;
        for (auto it = sorted.begin(); it != sorted.end();) {
            auto next = std::next(it);
            if (next == sorted.end()) break;
            if (*it == *next) {
                ret.push_back({it->first, it->second});
                it = next;
            } else if (it->second >= next->first) {
                if (it->second == next->first) {
                    ret.push_back({it->second, it->second});
                } else {
                    ret.push_back({next->first, std::min(it->second, next->second)});
                    // insert the remain interval to list
                    next->first = std::min(it->second, next->second);
                    next->second = std::max(it->second, next->second);
                }
                it = next;
            } else {
                it++;
            }
        }
        return ret;
    }
};