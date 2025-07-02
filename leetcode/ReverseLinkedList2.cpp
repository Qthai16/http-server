
struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

class Solution {
    ListNode *reverseList(ListNode *head) {
        if (head == nullptr) return head;
        ListNode *cur = head;
        ListNode *next = nullptr;
        ListNode *prev = nullptr;
        while (cur && cur->next) {
            next = cur->next;
            cur->next = prev;
            prev = cur;
            cur = next;
        }
        cur->next = prev;
        return cur;
    }

public:
    ListNode *reverseBetween(ListNode *head, int left, int right) {
        if (head == nullptr) return head;
        int cnt = 0;
        ListNode *prehead = nullptr, *subhead = nullptr;
        ListNode *subtail = nullptr, *posttail = nullptr;
        ListNode *cur = head;
        while (cur) {
            cnt++;
            if (cnt + 1 == left) {
                prehead = cur;
            } else if (cnt == left) {
                subhead = cur;
            }
            if (cnt == right) {
                subtail = cur;
            } else if (right + 1 == cnt) {
                posttail = cur;
            }
            cur = cur->next;
        }
        // importance: change subtail->next to nullptr before reverse
        // printf("prehead: %d, shead: %d, stail: %d, porttail: %d\n",
        //     prehead ? prehead->val : -1000,
        //     subhead ? subhead->val : -1000,
        //     subtail ? subtail->val : -1000,
        //     posttail ? posttail->val : -1000);
        if (subtail) subtail->next = nullptr;
        auto newhead = reverseList(subhead);
        if (prehead) prehead->next = newhead;
        if (subhead) subhead->next = posttail;
        return prehead == nullptr ? newhead : head;
    }
};