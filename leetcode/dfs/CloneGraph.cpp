// https://leetcode.com/problems/clone-graph

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <cstdlib>
#include <stack>
#include <cstdio>
using namespace std;

// Definition for a Node.
class Node {
public:
    int val;
    vector<Node *> neighbors;
    Node() {
        val = 0;
        neighbors = vector<Node *>();
    }
    Node(int _val) {
        val = _val;
        neighbors = vector<Node *>();
    }
    Node(int _val, vector<Node *> _neighbors) {
        val = _val;
        neighbors = _neighbors;
    }
};

class Solution {
public:
    Node *cloneGraph(Node *root) {
        if (root == nullptr)
            return root;
        // stack<Node *> s;
        unordered_map<Node *, Node*> visitted;
        Node *clone = new Node(root->val);
        auto dfs = [&](Node *n, Node *clone, auto &fn) {
            if (n == nullptr || visitted.count(n))
                return;
            // s.push(n);
            visitted[n] = clone;
            for (const auto &nei: n->neighbors) {
                Node *neiClone;
                auto it = visitted.find(nei);
                if (it == visitted.end()) {
                    neiClone = new Node(nei->val);
                    printf("new node: %d\n", nei->val);
                }
                else neiClone = it->second;
                clone->neighbors.push_back(neiClone);
                printf("link node %d vs %d\n", clone->val, neiClone->val);
                fn(nei, neiClone, fn);
            }
            // if (!s.empty()) s.pop();
        };
        auto ret = clone;
        dfs(root, clone, dfs);

        return ret;
    }
};