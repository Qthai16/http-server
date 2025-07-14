using namespace std;

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

ListNode *reverseList(ListNode *head) {
    if (head == nullptr) return head;
    ListNode *cur = head;
    ListNode *prev = nullptr;
    ListNode *next = nullptr;
    while (cur && cur->next) {
        next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    cur->next = prev;
    return cur;
}