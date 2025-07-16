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

#include <vector>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <cstdio>
#include <iostream>
using namespace std;

struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

// for every sub-tree: left < root < right
class Solution {
    void dfs(TreeNode *node, stack<TreeNode *> &s, vector<pair<int, TreeNode *>> &arr) {
        if (node == nullptr) {
            if (!s.empty()) s.pop();
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

    void morrisTraversal(TreeNode *node, pair<TreeNode *, TreeNode *>& swapPair) {
        // traversal + compare prev->val vs cur->val
        TreeNode *prev{nullptr}, *curr{nullptr}, *node1{nullptr}, *node2{nullptr};
        curr = node;
        while (curr != nullptr) {
            if (curr->left == nullptr) {
                // printf("(%d)", curr->val);
                node1 = node2;
                node2 = curr;
                curr = curr->right;
            } else {
                prev = curr->left;
                while (prev->right != nullptr && prev->right != curr)
                    prev = prev->right;

                // Make curr as the right child of its previous
                if (prev->right == nullptr) {
                    prev->right = curr;
                    curr = curr->left;
                } else {// fix the right child of previous
                    prev->right = nullptr;
                    // printf("[%d]", curr->val);
                    node1 = node2;
                    node2 = curr;
                    curr = curr->right;
                }
            }
            if (node1 && node2 && node1->val > node2->val) {
                // printf("got pair (%d): (%d)\n", node1->val, node2->val);
                if (!swapPair.first && !swapPair.second) {
                    swapPair.first = node1;
                    swapPair.second = node2;
                } else {
                    swapPair.second = node2;
                }
            }
        }
    }

    void swapNode(TreeNode *l, TreeNode *r) {
        swap(l->val, r->val);
    }

public:
    void recoverTree(TreeNode *root) {
        // solution 1: use stack + recursion in order traversal, then check sorted array
        if (root == nullptr) return;
        stack<TreeNode *> s;
        vector<pair<int, TreeNode *>> arr;
        s.push(root);
        while (!s.empty()) {
            dfs(s.top(), s, arr);
        }
        vector<int> ind;
        for (auto i = 0; i < arr.size(); i++) {
            if (i + 1 >= arr.size()) break;
            if (arr[i].first < arr[i + 1].first) continue;
            ind.push_back(i);
        }
        if (ind.size() == 1)
            swapNode(arr[ind[0]].second, arr[ind[0] + 1].second);
        else if (ind.size() == 2)
            swapNode(arr[ind[0]].second, arr[ind[1] + 1].second);
    }

    void recoverTree(TreeNode *root) {
    // solution 2: use morris traversal + compare last node with current node
        if (root == nullptr) return;
        pair<TreeNode *, TreeNode *> swapNodes{nullptr, nullptr};
        morrisTraversal(root, swapNodes);
        // fflush(stdout);
        if (swapNodes.first && swapNodes.second) {
            swapNode(swapNodes.first, swapNodes.second);
        }
    }
};
