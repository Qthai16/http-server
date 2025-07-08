class Solution {
public:
    string removeKdigits(string num, int k) {
        if (k == num.size()) return "0";
        const int base = '0';
        deque<int> smaller;
        deque<int> s;// mono stack, increasing (next smaller)
        for (auto i = 0; i < num.size(); i++) {
            while (!s.empty() && num[i] < num[s.back()]) {
                smaller.push_back(s.back());
                s.pop_back();
            }
            s.push_back(i);
        }
        auto cnt = 0;
        while (cnt < k) {
            if (!smaller.empty()) {
                num[*smaller.begin()] = 'x';
                smaller.pop_front();
            } else {// num is string of increasing number
                auto cp = 0;
                while (num[num.size() - cp - 1] == 'x') {
                    cp++;
                }
                num[num.size() - cp - 1] = 'x';
            }
            cnt++;
        }
        // printf("%s\n", num.c_str());
        string ret;
        bool a = false;
        for (auto i = 0; i < num.size(); i++) {
            if (!a && (num[i] == '0' || num[i] == 'x')) continue;
            if (!a) a = true;
            if (num[i] != 'x') ret += num[i];
        }
        return !ret.empty() ? ret : "0";
    }
};