#include <vector>
#include <string>
#include <unordered_map>

using namespace std;

class Solution {
public:
    int longestPalindrome(string s) {
        int maxOdd = 0;
        unordered_map<char, int> tb;
        for (const auto &c: s) {
            tb[c] += 1;
        }
        int cntEven = 0;
        int allEven = 0;
        bool middle = false;
        for (const auto &p: tb) {
            if (p.second % 2) {
                allEven += p.second - 1;
                maxOdd = max(maxOdd, p.second);
                middle = true;
            } else {
                cntEven += p.second;
                allEven += p.second;
            }
        }
        return max(allEven + static_cast<int>(middle), maxOdd + cntEven);
    }
};