class Solution {
public:
    bool hasCycle(ListNode *head) {
        // fast and slow pointer
        auto fast = head;
        auto slow = head;
        if (head == nullptr || head->next == nullptr)
            return false;
        while (fast->next && fast->next->next) {
            fast = fast->next->next;
            slow = slow->next;
            if (fast == slow)
                return true;
        }
        return false;
    }
};