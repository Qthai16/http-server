double findMaxAverage(vector<int> &nums, int k) {
    if (k >= nums.size()) k = nums.size();
    int64_t sum = 0;
    double avg = 0;
    for (auto i = 0; i < k; i++) {
        sum += nums[i];
    }
    avg = static_cast<double>(sum) / static_cast<double>(k);
    for (auto i = 1; i + k - 1 < nums.size(); i++) {
        sum += nums[i + k - 1] - nums[i - 1];
        avg = std::max(avg, static_cast<double>(sum) / static_cast<double>(k));
    }
    return avg;
}