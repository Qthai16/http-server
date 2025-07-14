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

struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

#include <vector>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <cstdio>
using namespace std;

// for every sub-tree: left < root < right
class Solution {
    void dfs(TreeNode *node, stack<TreeNode*>& s, vector<pair<int, TreeNode*>>& arr) {
        if (node == nullptr) {
            if(!s.empty()) s.pop();
            return;
        }
        if (node->left == nullptr && node->right == nullptr) {
            arr.push_back({node->val, node});
            if (!s.empty()) s.pop();
            return;
        }
        if (node->left) {
            s.push(node->left);
            dfs(node->left, s, arr);
        }
        arr.push_back({node->val, node});
        if (node->right) {
            s.push(node->right);
            dfs(node->right, s, arr);
        }
        if (!s.empty()) s.pop();
    }
    void swapNode(TreeNode* l, TreeNode* r) {
        swap(l->val, r->val);
    }
public:
    void recoverTree(TreeNode* root) {
        if (root == nullptr) return;
        stack<TreeNode*> s;
        vector<pair<int, TreeNode*>> arr;
        s.push(root);
        while (!s.empty()) {
            dfs(s.top(), s, arr);
        }
        vector<int> ind;
        for (auto i = 0; i < arr.size(); i++) {
            if (i + 1 >= arr.size()) break;
            if (arr[i].first < arr[i+1].first) continue;
            ind.push_back(i);
        }
        if (ind.size() == 1)
            swapNode(arr[ind[0]].second, arr[ind[0]+1].second);
        else if (ind.size() == 2)
            swapNode(arr[ind[0]].second, arr[ind[1]+1].second);
    }
};