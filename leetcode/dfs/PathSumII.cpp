#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <cstdlib>
#include <cstdio>

using namespace std;

//  * Definition for a binary tree node.
struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

class Solution {
public:
    vector<vector<int>> pathSum(TreeNode *root, int targetSum) {
        if (root == nullptr) return {};
        vector<vector<int>> ret;
        deque<TreeNode *> s;
        int currSum = 0;
        auto inorderTraversal = [&](TreeNode *node, auto &fn) {
            if (node == nullptr) return;
            s.push_back(node);
            currSum += s.back()->val;
            if (node->left == nullptr && node->right == nullptr) {
                // leaf node
                printf("cur sum: %d, s.back: %d\n", currSum, s.back()->val);
                if (currSum == targetSum) {
                    vector<int> tmp(s.size());
                    for (auto i = 0; i < s.size(); i++) {
                        tmp[i] = s[i]->val;
                    }
                    ret.push_back(std::move(tmp));
                }
                if (!s.empty()) {
                    currSum -= s.back()->val;
                    s.pop_back();
                }
                return;
            }
            if (node->left) fn(node->left, fn);
            if (node->right) fn(node->right, fn);

            if (!s.empty()) {
                currSum -= s.back()->val;
                s.pop_back();
            }
        };

        inorderTraversal(root, inorderTraversal);
        return ret;
    }
};