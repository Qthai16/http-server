class Solution {
public:
    int lengthOfLongestSubstring(string s) {
        std::unordered_map<char, int> lk;
        int ret = 0;
        if (s.empty()) return 0;
        if (s.size() == 1) return 1;
        for (auto i = 0; i < s.size();) {
            auto it = lk.find(s[i]);
            if (it == lk.end()) {
                lk.emplace(s[i], i);
                i++;
            } else {
                i = it->second + 1;
                ret = std::max(ret, static_cast<int>(lk.size()));
                lk.clear();
            }
        }
        return std::max(ret, static_cast<int>(lk.size()));
    }
};


class Solution {
public:
    int lengthOfLongestSubstring(string s) {
        std::unordered_set<char> lk;
        int ret = 0;
        int64_t left = 0;
        if (s.empty()) return 0;
        if (s.size() == 1) return 1;
        for (auto i = 0; i < s.size(); i++) {
            auto it = lk.find(s[i]);
            if (it == lk.end()) {
                lk.insert(s[i]);
            } else {
                while (s[left] != s[i]) {
                    // increase left and remove from lk
                    lk.erase(s[left]);
                    left++;
                }
                // here left point to duplicate character, increase it
                left++;
            }
            ret = std::max(ret, static_cast<int>(i - left + 1));
        }
        return ret;
    }
};