int romanToInt(string s) {
    char last = '\0';
    int ret = 0;
    auto lookup = unordered_map<char, int>{
        {'I', 1},
        {'V', 5},
        {'X', 10},
        {'L', 50},
        {'C', 100},
        {'D', 500},
        {'M', 1000}
    };
    for (const auto& e : s) {
        auto iter = lookup.find(e);
        if (iter == lookup.end()) continue;
        ret += iter->second;
        if (last == 'I' && (e == 'V' || e == 'X'))
            ret -= 2;
        else if (last == 'X' && (e == 'L' || e == 'C'))
            ret -= 20;
        else if (last == 'C' && (e == 'D' || e == 'M'))
            ret -= 200;
        last = e;
    }
    return ret;
}

int romanToInt(string s) {
        int res = 0;
        unordered_map<char, int> roman = {
            {'I', 1},
            {'V', 5},
            {'X', 10}, 
            {'L', 50},
            {'C', 100},
            {'D', 500},
            {'M', 1000}
        };

        for (int i = 0; i < s.size() - 1; i++) {
            if (roman[s[i]] < roman[s[i + 1]]) {
                res -= roman[s[i]];
            } else {
                res += roman[s[i]];
            }
        }

        return res + roman[s[s.size() - 1]];        
    }