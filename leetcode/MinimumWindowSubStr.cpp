#include <string>
#include <queue>
#include <unordered_map>
using namespace std;

// Input: s = "ADOBECODEBANC", t = "ABC"
// Output: "BANC"

// Input: s = "abc", t = "b"
// Output: "b"

// s = "bdab", t = "ab"
// output: "ab"

// todo: bug, shrink sliding windows wrong

class Solution {
public:
    string minWindow(string s, string t) {
        unordered_map<char, int> orglk;
        for (const auto &c: t) {
            auto it = orglk.find(c);
            if (it == orglk.end()) orglk[c] = 1;
            else
                it->second += 1;
        }
        int l = 0, r = 0;
        int retl = 0;
        int minwin = s.size();
        bool found = false;
        auto checkWindow = [&](const unordered_map<char, int> &tb) {
            if (!tb.empty()) return;
            auto size = r - l;
            if (size <= minwin) {
                minwin = size;
                retl = l;
                found = true;
            }
        };
        while (l < s.size()) {
            auto lk = orglk;
            bool keepl = false;
            std::queue<int> lastPos;
            while (r < s.size()) {
                auto it = lk.find(s[r]);
                if (it == lk.end()) {
                    // not contain char in t
                    if (orglk.find(s[r]) != orglk.end()) {
                        // this char appear in t, keep its index for future iteration
                        lastPos.push(r);
                    }
                    if (!keepl) {
                        l++;
                    }
                    r++;
                } else {
                    // decrease lk char count
                    lastPos.push(r);
                    it->second -= 1;
                    if (it->second == 0) lk.erase(it);
                    r++;
                    if (!keepl) keepl = true;// keep l position
                }
                if (lk.empty()) {
                    // we found a window
                    checkWindow(lk);
                    // decrease l until lastPos empty
                    while (!lastPos.empty()) {
                        auto it1 = lk.find(s[l]);
                        if (it1 != lk.end()) it1->second += 1;
                        else lk[s[l]] = 1;
                        lastPos.pop();
                        if (lastPos.empty()) break;
                        l = lastPos.front();
                        auto it = lk.find(s[l]);
                        if (it != lk.end()) {
                            lk.erase(it);
                            checkWindow(lk);
                        }
                    }
                }
            }
            // r is pointing to the next match character
            l = r;
        }
        return found ? s.substr(retl, minwin) : string{};
    }
};