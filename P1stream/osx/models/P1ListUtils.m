#import "P1ListUtils.h"


void insertSourceInListAtIndex(P1ListNode *head, P1Source *src, NSUInteger index)
{
    NSUInteger count = 0;
    P1ListNode *node;

    p1_list_iterate(head, node) {
        if (count == index) {
            p1_list_before(node, &src->link);
            return;
        }
        else {
            count++;
        }
    }

    // Fell-through, simply append.
    p1_list_before(head, &src->link);
}

void removeSourceFromListAtIndex(P1ListNode *head, NSUInteger index)
{
    NSUInteger count = 0;
    P1ListNode *node;

    p1_list_iterate(head, node) {
        if (count == index) {
            p1_list_remove(node);
            break;
        }
        else {
            count++;
        }
    }
}
