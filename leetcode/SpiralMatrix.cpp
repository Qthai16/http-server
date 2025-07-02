enum Act {
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3,
    UP = 4,
};
vector<int> spiralOrder(vector<vector<int>>& matrix) {
    std::vector<Act> acts = {Act::RIGHT, Act::DOWN, Act::LEFT, Act::UP};
    // action: j + 1, i + 1, j - 1, i -1
    vector<int> ret;
    int i = 0;
    int j = 0;
    auto actInd = 0;
    auto maxj = matrix[0].size();
    auto maxi = matrix.size();
    auto maxVisited = static_cast<long>(maxi)*static_cast<long>(maxj);
    auto visited = 0;
    while (visited < maxVisited) {
        ret.push_back(matrix[i][j]);
        visited++;
        matrix[i][j] = -1000; // mark visited
        if (acts[actInd%acts.size()] == Act::RIGHT) {
            if (j + 1 >= maxj || matrix[i][j+1] == -1000) {
                actInd++;
                i = std::min(i + 1, static_cast<int>(maxi)); // go down
            } else j += 1;
        } else if (acts[actInd%acts.size()] == Act::DOWN) {
            if (i + 1 >= maxi || matrix[i+1][j] == -1000) {
                actInd++;
                j = std::max(0, j - 1); // go left
            } else i += 1;
        } else if (acts[actInd%acts.size()] == Act::LEFT) {
            if (j - 1 < 0 || matrix[i][j-1] == -1000) {
                actInd++;
                i = std::max(0, i - 1);
            } else j -= 1;
        } else if (acts[actInd%acts.size()] == Act::UP) {
            if (i - 1 < 0 || matrix[i-1][j] == -1000) {
                actInd++;
                j = std::min(j + 1, static_cast<int>(maxj));
            } else i -= 1;
        }
    }
    return ret;
}