# Final Project Design Log
**Author:** Ahmad Khalifa · **P2 code used:** instructor reference · **P3 code used:** instructor reference
**Tools used:** Used Claude to look through the finalized project for bugs.
It pointed out a handful of bugs and explained the causes, along with some edge cases.
All code was written by me, there were no examples or code snippets provided by AI.

<!-- 700–1,000 words. An argument, not a diary: say what you decided, what
     you rejected, and what evidence backs it up. Paste the numbers your
     tests print. Delete these comments before submitting. -->

## 1. LRU cache: why get and put are O(1)
<!-- List every pointer change in (a) a get() hit on a middle node and
     (b) a put() that evicts. Then argue O(1). -->
When a call to get() hits a middle node, said node is first unlinked
using a private helper function, then pushed to the top. Say we have the
following nodes: n1 -> n2 -> n3. The unlink helper makes n1's next point
to n3, and n3's prev point to n1. Then, the node is given to another
private helper that pushes the node to the top of the linked list,
push\_front(), replacing head. So n2's next points to n1, and n2's prev
is null. The new order is n2 -> n1 -> n3.

A call to put() is forced to evict when the key is not already cached,
and map\_.size() is larger than capacity\_. In such a case, we first
check if the cache is empty (which is possible when capacity_ = 0); If
it is, the function returns early and nothing is done. Otherwise, the
tail is passed to the unlink helper function (where n-\>prev-\>next is
changed to null, and tail\_ is changed to n-\>prev), and its key is
removed from map\_, and the evictions statistic is incremented. Next,
instead of freeing the node, it's used to host the new entry's key value
pair. The new key value pair is inserted into map\_, and the node gets
passed to the push\_front() helper.

The LRU cache being O(1) largely depends on std::unordered\_map being
O(1). Looking at the functions one by one, the get() function uses
std::unordered\_map::find(), and assuming that's O(1), all the function
does is move the found node (if any) to the top, which, as described
above, simply moves pointers around, it does not depend on the content
of said node hence it is O(1). Similarly, the put() function searches
for a node to check if it already exists, if it does, it moves it to the
top and returns. Otherwise a new node is allocated and inserted into
the map, which is also O(1). The eviction case, as described above,
calls the unlink helper and std::unordered\_map::erase(), which are both
O(1). The same reasoning applies to contains() and erase(). The only
function that isn't actually O(1) is clear(), it loops over all the
nodes in the cache so it's actually O(n).

## 2. Heap and top\_k
<!-- Why is top_k O(n log k)? One measurement against a full sort. -->
The first thing top\_k does is bound k to the size of the items vector,
then loop over it, pushing each item to the min heap. Since the push
operation on a min heap is O(log n) (because it may need to traverse
through a root to leaf path during a push), and we are doing it k times,
the time complexity of the loop ends up being O(k log k). Then, another
loop goes from k to items.size(). It compares the item against top(),
which is O(1). If the item wins, the top of the heap is popped, and the
new element pushed, which are both O(log k). The heap is then emptied
into an std::vector (O(k log k)), then the vector is reversed (O(k)),
and returned. So the worst case is O(n log k).

Sorting 100,000 random numbers:
| top\_k(k = 10) | std::sort |
| -------------- | --------- |
|   20.288468ms  | 167.715151ms |

## 3. BST height
<!-- Your printed heights for ids 1..1000 inserted in order vs. shuffled.
     Why do sequential ids degenerate the tree, what does it do to find(),
     and what would fix it? -->
Sequential ids degenerate the BST since they turn it into a linked list.
For every inserted node, the next node will always be on its right, and
so on. This also makes it so find() ends up being O(n), since it would
just be iterating through the tree starting at the root and going to the
right child over and over until the query is found.

Height of tree with sequential numbers from 1..1000, vs shuffled numbers
from 1..1000:
| Sequential | Shuffled |
| ---------- | -------- |
|    1000    |    21    |

As can be seen above, the height of the tree sigificantly decreases if
the numbers are inserted randomly instead of sequentially.

This can be fixed by turning the tree into an AVL, which would balance
itself after every insert, making sure there is at most one extra node
on either side of a given node. This would turn the time complexity of
find() from the example above from O(n) to O(log n).

## 4. Three colors in DFS
<!-- Why gray and black must be different. Give a small graph where a
     visited/not-visited DFS would report a cycle that is not there. -->
When preforming DFS, gray and black must be different since previously
visited nodes must be differentiated by whether they are on the current
path or not. If a node has already been visited when we run into it
again, it could be because there's more than one path to said node, not
necessarily that there's a cycle. A "gray" node is one that's on the
current path, a "black" node is one that has been visited before, but is
not on the current path.

A DFS implementation that doesn't account for this would falsely report
a diamond shaped graph as containing a cycle. For example:
a -> b, a -> c, c -> d, b -> d. When such a DFS implementation searches
through this, it would report a cycle when d is hit again, which is not
true, there are just multiple paths to d.

## 5. Inverted index vs. linear scan
<!-- Your timing table at 100 / 1,000 / 5,000 documents, and what the trend
     shows. -->

| documents | index (ms/query) | linear (ms/query) |
|---|---|---|
| 100 | 1.580295ms | 7.767531ms |
| 1,000 | 11.840478ms | 64.706936ms |
| 5,000 | 73.347429ms | 436.649589ms |

The trend shows both times, index and linear, growing proportionally to
the number of documents. The inverted index is, as expected, faster.

This is because the linear search re-tokenizes every document on every
query.

The reason the times may be closer than the expected 1,500x improvement
in the spec is that the number of unique words used was small.

## 6. Threat model
<!-- 3–5 sentences: what does the default policy chain defend against, and
     what does it NOT? (Hint: python3 is on the allowlist.) -->
The default policy chain defends against known bad commands, e.g
"rm -rf". It also disallows various patterns used in shell scripting,
which can stop the LLM from doing something like executing a fork bomb.

However, it does not defend against malicious input to the allowed
programs. For example, since the LLM can run python, it could craft a
bad script then pass it to python. Of course, this would prompt the user
before executing anything, but it's still a liability. The same goes for
make, arbitrary programs can be run from makefiles, which allows the LLM
to get around the restricted programs list pretty easily.

## 7. What I would design differently

Post order delete in MemoryIndex::clear() instead of calling remove() on
each node. This would have been more efficient.
