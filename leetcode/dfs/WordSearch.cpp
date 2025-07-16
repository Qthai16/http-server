#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <string>
#include <stack>
#include <set>
#include <algorithm>
#include <cstdlib>
using namespace std;

// Given an m x n grid of characters board and a string word, return true if word exists in the grid.
// The word can be constructed from letters of sequentially adjacent cells, where adjacent cells are horizontally or vertically neighboring.
// The same letter cell may not be used more than once.

class Solution {
private:
    int maxj{0};
    int maxi{0};

private:
    // void dfs(stack<pair<int, int>> &nodes, vector<vector<char>> &board, unordered_map<int, vector<int>> &visited, const string &word, bool &found) {
    //     if (nodes.empty()) {
    //         return;
    //     }
    //     if (found) return;
    //     auto i = nodes.top().first;
    //     auto j = nodes.top().second;
    //     if (i >= maxi || i < 0 || j < 0 || j >= maxj) return;
    //     // printf("%d %d %zu\n", i, j, nodes.size());
    //     if (board[i][j] != word[nodes.size() - 1]) {
    //         if (!nodes.empty()) {
    //             auto ri = nodes.top().first;
    //             auto rj = nodes.top().second;
    //             nodes.pop();
    //             auto it = find(visited[ri].begin(), visited[ri].end(), rj);
    //             if (it != visited[ri].end()) visited[ri].erase(it);
    //         }
    //         return;
    //     }
    //     visited[i].push_back(j);
    //     // printf("  %c\n", board[i][j]);
    //     if (word.size() == nodes.size() && word[word.size() - 1] == board[nodes.top().first][nodes.top().second]) {
    //         found = true;
    //         return;
    //     }
    //     if (j + 1 < maxj &&
    //         (find(visited[i].begin(), visited[i].end(), j + 1) == visited[i].end())) {// right
    //         nodes.push({i, j + 1});
    //         dfs(nodes, board, visited, word, found);
    //     }
    //     if (i + 1 < maxi && (find(visited[i + 1].begin(), visited[i + 1].end(), j) == visited[i + 1].end())) {// down
    //         nodes.push({i + 1, j});
    //         dfs(nodes, board, visited, word, found);
    //     }
    //     if (j - 1 >= 0 && (find(visited[i].begin(), visited[i].end(), j - 1) == visited[i].end())) {// left
    //         nodes.push({i, j - 1});
    //         dfs(nodes, board, visited, word, found);
    //     }
    //     if (i - 1 >= 0 && (find(visited[i - 1].begin(), visited[i - 1].end(), j) == visited[i - 1].end())) {// up
    //         nodes.push({i - 1, j});
    //         dfs(nodes, board, visited, word, found);
    //     }

    //     if (!nodes.empty()) {
    //         auto ri = nodes.top().first;
    //         auto rj = nodes.top().second;
    //         nodes.pop();
    //         auto it = find(visited[ri].begin(), visited[ri].end(), rj);
    //         if (it != visited[ri].end()) visited[ri].erase(it);
    //     }
    // }

public:
    // bool exist(vector<vector<char>> &board, string word) {
    //     if (word.size() > board.size() * board[0].size())
    //         return false;
    //     stack<pair<int, int>> nodes;
    //     unordered_map<char, vector<pair<int, int>>> tb;
    //     for (auto i = 0; i < board.size(); i++) {
    //         for (auto j = 0; j < board[i].size(); j++) {
    //             tb[board[i][j]].push_back({i, j});
    //         }
    //     }
    //     for (const auto &c: word) {
    //         if (tb.find(c) == tb.end())
    //             return false;
    //     }
    //     unordered_map<int, vector<int>> visited;
    //     maxj = board[0].size();
    //     maxi = board.size();
    //     for (const auto &p: tb[word[0]]) {
    //         nodes.push(p);
    //         auto found = false;
    //         dfs(nodes, board, visited, word, found);
    //         if (found)
    //             return true;
    //         visited.clear();
    //         while (!nodes.empty()) nodes.pop();
    //     }
    //     return false;
    // }

    bool exist(vector<vector<char>> &board, string word) {
        // this one is slower than the prev
        if (word.size() > board.size() * board[0].size())
            return false;
        stack<pair<int, int>> nodes;
        unordered_map<char, vector<pair<int, int>>> tb;
        for (auto i = 0; i < board.size(); i++) {
            for (auto j = 0; j < board[i].size(); j++) {
                tb[board[i][j]].push_back({i, j});
            }
        }
        for (const auto &c: word) {
            if (tb.find(c) == tb.end())
                return false;
        }
        unordered_set<string> visited; // ij
        maxj = board[0].size();
        maxi = board.size();
        auto dfs = [&](int i, int j, int& k, auto& fn) -> bool {
            if (i >= maxi || i < 0 || j >= maxj || j < 0 || visited.count(std::to_string(i) + std::to_string(j))) {
                return false;
            }
            if (board[i][j] != word[k]) {
                return false;
            }
            k++;
            visited.insert(std::to_string(i) + std::to_string(j));
            // printf("%c\n", board[i][j]);
            if (k >= word.size()) {
                return true;
            }
            // right, down, left, up
            auto ret = fn(i, j + 1, k, fn) || fn(i + 1, j, k, fn) || fn(i, j - 1, k, fn) || fn(i - 1, j, k, fn);
            if (!ret) {
                visited.erase(std::to_string(i) + std::to_string(j));
                k--;
            }
            return ret;
        };
        int k = 0;
        for (const auto &p: tb[word[0]]) {
            auto ret = dfs(p.first, p.second, k, dfs);
            fflush(stdout);
            if (ret) return true;
            visited.clear();
            k = 0;
        }
        return false;
    }
};