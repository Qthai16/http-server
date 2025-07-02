class Solution {
public:
    string longestCommonPrefix(vector<string>& strs) {
        if (strs.empty()) return {};
        int minLen = 0xFFFFFFFF;
        string ret;
        for (const auto& s : strs) {
            if (s.size() < minLen) minLen = s.size();
        }
        char c = '\0';
        for (auto i = 0; i < minLen; i++) {
            c = strs[0][i];
            for (auto j = 1; j < strs.size(); j++) {
                if (strs[j][i] != c) return ret;
            }
            ret += c;
        }
        return ret;
    }
};