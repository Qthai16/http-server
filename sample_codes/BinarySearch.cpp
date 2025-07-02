int lower_bound(int* a, int64_t start, int64_t end, int value) {
    // return index of first element >= value
    auto low = start;
    auto high = end;
    for (low < high) {
        mid = (low + high)/2;
        if (a[mid] < value) { // change to <= for upper bound
            low = mid + 1;
        } else { // a[mid] >= value
            high = mid;
        }
    }
    return low;
}

int binary_search(int* a, int64_t start, int64_t end, int value) {
    auto i = lower_bound(a, start, end, value);
    return i < end && a[i] == value;
}
