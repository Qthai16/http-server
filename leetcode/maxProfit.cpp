class Solution {
public:
    int maxProfit(vector<int>& prices) {
        int maxp = 0;
        auto l = 0;
        for (auto i = 1; i < prices.size(); i++) {
            if (prices[i] <= prices[l]) { // price is lower, update low index
                l = i;
            } else { // sell and compare with max profit
                maxp = std::max(maxp, prices[i] - prices[l]);
            }
        }
        return maxp;
    }
};