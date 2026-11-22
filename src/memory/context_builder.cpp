// src/memory/context_builder.cpp — YOU IMPLEMENT. Spec §4.4.

#include "memory/context_builder.h"
#include "util/min_heap.h"

ContextPlan select_context(const std::vector<ContextItem>& items,
                           std::size_t budget, std::size_t keep_recent) {
    // total the tokens; if they fit, keep everything. Otherwise push
    // every unprotected item onto a MinHeap keyed on (drop_rank(role), index)
    // and pop/drop until the total fits or the heap is empty.
    //
    // Clues:
    //  * Heap entries:  MinHeap<std::pair<int, std::size_t>> heap;
    //    std::pair compares .first, then .second, which is exactly
    //    "lowest rank first, then oldest first".
    //  * "The last keep_recent items" start at index items.size() - keep_recent,
    //    but these are unsigned: if keep_recent > items.size() that
    //    subtraction wraps around to a huge number. Handle that case first.
    //  * A vector<bool> keep(items.size(), true) makes the final pass easy:
    //    collect the indices still marked true, in ascending order.
    //  * total_tokens is the sum of the KEPT items; over_budget is
    //    total_tokens > budget.
    MinHeap<std::pair<int, std::size_t>> heap;
    MinHeap<std::size_t> idxes;
    ContextPlan ret;

    for (std::size_t i = 0; i < items.size(); i++) {
	    ret.keep.push_back(i);
	    ret.total_tokens += items[i].tokens;
    }

    if (ret.total_tokens <= budget) {
	    ret.over_budget = false;
	    return ret;
    }

    /* Make sure keep_recent isn't > items.size(). */
    keep_recent = std::min(keep_recent, items.size());

    /* Push all unprotected items onto the min heap. */
    for (std::size_t i = 0; i < items.size() - keep_recent; i++) {
	    if (items[i].role == Role::System || items[i].pinned)
		    continue;
	    heap.push({drop_rank(items[i].role), i});
    }

    while (ret.total_tokens > budget && !heap.empty()) {
	    std::size_t top = heap.pop().second;

	    ret.total_tokens -= items[top].tokens;
	    /*
	     * After each element is removed, later elements
	     * need to be offset by one. This can be accounted
	     * for by sorting the elements to pop using another
	     * MinHeap.
	     */
	    idxes.push(top);
    }

    /* Erase the dropped elements, accounting for the drift using i. */
    for (std::size_t i = 0; !idxes.empty(); i++)
	    ret.keep.erase(ret.keep.begin() + idxes.pop() - i);

    ret.over_budget = ret.total_tokens > budget;
    return ret;
}
