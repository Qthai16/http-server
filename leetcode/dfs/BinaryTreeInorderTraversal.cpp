#include <vector>
#include <unordered_set>
#include <stack>

/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *     int val;
 *     TreeNode *left;
 *     TreeNode *right;
 *     TreeNode() : val(0), left(nullptr), right(nullptr) {}
 *     TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
 *     TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
 * };
 */

using namespace std;

struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

// in order: left -> root -> right

class Solution {
    void dfs(TreeNode *node, vector<int>& ret, stack<TreeNode*>& s) {
        if (node == nullptr) {
            if(!s.empty()) s.pop();
            return;
        }
        if (node->left == nullptr && node->right == nullptr) {
            ret.push_back(node->val);
            if (!s.empty()) s.pop();
            return;
        }
        if (node->left) {
            s.push(node->left);
            dfs(node->left, ret, s);
        }
        ret.push_back(node->val);
        if (node->right) {
            s.push(node->right);
            dfs(node->right, ret, s);
        }
        if (!s.empty()) s.pop();
    }
public:
    vector<int> inorderTraversal(TreeNode *root) {
        vector<int> ret;
        stack<TreeNode* > s;
        s.push(root);
        while (!s.empty()) {
            dfs(s.top(), ret, s);
        }
        return ret;
    }
};