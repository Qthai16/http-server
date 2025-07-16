class Solution {
public:
    int minAddToMakeValid(string s) {
        if (s.empty()) return 0;
        int k = 0;
        stack<char> list;
        for (auto i = 0; i < s.size(); i++) {
            if (!list.empty() && list.top() == '(' && s[i] == ')') {
                list.pop();
                continue;
            }
            if (s[i] == ')') {
                k += 1;
                continue;
            }
            list.push(s[i]);
        }
        return k + list.size();
    }
};