bool isPalindrome(int x) {
    if (x < 0) return false;
    auto num = x;
    std::vector<int> digits;
    auto base = 1;
    do {
        digits.push_back(num%10);
        num = num/10;
    } while(num > 0);
    for (int i = 0, j = digits.size() -1; i <= j; i++, j--) {
        if (digits[i] != digits[j]) {
            return false;
        }
    }
    return true;
}

bool isPalindrome(int x) {
    if (x < 0) return false;
    auto num = x;
    int64_t reverse = 0;
    while (num > 0) {
        reverse = reverse*10 + num%10;
        num /= 10;
    }
    return reverse == x;
}