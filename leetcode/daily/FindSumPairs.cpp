class FindSumPairs {
    vector<int>& s1;
    vector<int>& s2;
    unordered_map<int, vector<int>> lks2;
public:
    FindSumPairs(vector<int>& nums1, vector<int>& nums2): s1(nums1), s2(nums2), lks2() {
        sort(s1.begin(), s1.end());
        for (auto i = 0; i < nums2.size(); i++) {
            lks2[nums2[i]].push_back(i);
        }
        // sort list index
        for (auto it = lks2.begin(); it != lks2.end(); it++) {
            sort(it->second.begin(), it->second.end());
        }
    }
    
    void add(int index, int val) {
        auto v = s2[index];
        s2[index] += val;
        auto it = lks2.find(v);
        if (it != lks2.end()) {
            auto indIt = std::lower_bound(it->second.begin(), it->second.end(), index);
            if (indIt != it->second.end()) it->second.erase(indIt);
        }
        lks2[v + val].push_back(index);
        sort(lks2[v + val].begin(), lks2[v + val].end());
    }
    
    int count(int tot) {
        int ret = 0;
        for (auto i = 0; i < s1.size(); i++) {
            auto num = tot - s1[i];
            auto it = lks2.find(num);
            if (it != lks2.end()) ret += it->second.size();
        }
        return ret;
    }
};