// src/memory/memory_index.cpp — YOU IMPLEMENT. Spec §4.3.

#include "memory/memory_index.h"

MemoryIndex::~MemoryIndex() {
    // free every node (post-order: both children before the node).
    clear();
}

bool MemoryIndex::insert(MemoryRecord rec) {
    // walk down from root_ (smaller ids left, larger right). Equal id:
    // return false and change nothing. Otherwise attach a new Node where the
    // walk fell off the tree, ++count_, return true.
    // Clue: memory ids arrive as 1, 2, 3, ..., so the tree is one long right
    // spine. A loop handles 5,000 nodes easily; so does recursion.
    auto search = find_internal(rec.id);
    Node *n;

    if (search.second != nullptr)
	    return false;

    count_++;
    n = search.first;
    if (n != nullptr) {
	    if (n->rec.id < rec.id) {
		    n->right = new Node;
		    n = n->right;
	    } else {
		    n->left = new Node;
		    n = n->left;
	    }
    } else {
	    /* If there's no root, allocate one. */
	    n = new Node;
	    root_ = n;
    }

    n->rec = std::move(rec);
    n->left = n->right = nullptr;
    return true;
}

const MemoryRecord* MemoryIndex::find(std::uint64_t id) const {
    auto search = find_internal(id);

    return search.second != nullptr ? &search.second->rec : nullptr;
}

bool MemoryIndex::remove(std::uint64_t id) {
    // leaf, one child, two children (use the in-order successor or
    // predecessor). Spec Appendix C draws all three.
    // Clue: the successor (leftmost node of the right subtree) has no left
    // child but MAY have a right child: that child takes its place.
    // Clue: --count_ exactly once per successful remove. Counting inside
    // the helper AND here double-counts the two-children case.
    // Clue: when the removed node is the root, root_ itself must change.
    auto search = find_internal(id);
    Node *n, *parent, *suc;

    parent = search.first;
    n = search.second;

    if (n == nullptr)
	    return false;

    if (n->left == nullptr && n->right == nullptr)
	    suc = nullptr;
    else if (n->left == nullptr)
	    suc = n->right;
    else if (n->right == nullptr)
	    suc = n->left;
    else {
	    Node *prev = n;

	    /* Find the in-order successor. */
	    for (suc = n->right; suc->left != nullptr; suc = suc->left)
		    prev = suc;

	    /*
	     * Since the successor's left child is null,
	     * its successor is its right child, if any.
	     */
	    if (prev->left == suc)
		    prev->left = suc->right;
	    else
		    prev->right = suc->right;

	    suc->left = n->left;
	    suc->right = n->right;
    }

    if (parent == nullptr) {
	    /* If the parent is null, suc should be the root. */
	    root_ = suc;
    } else {
	    if (parent->left == n)
		    parent->left = suc;
	    else
		    parent->right = suc;
    }

    delete n;
    count_--;
    return true;
}

std::size_t MemoryIndex::size() const noexcept { return count_; }

std::size_t MemoryIndex::height() const {
    // empty tree = 0, one node = 1, otherwise 1 + the taller child.
    return height_of(root_);
}

void MemoryIndex::for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const {
    // left subtree, then fn(node->rec), then right subtree.
    if (root_ == nullptr)
	    return;
    for_each_in_order_recursive(root_, fn);
}

void MemoryIndex::clear() {
    // Clue: after freeing the nodes, set root_ = nullptr and count_ = 0, or
    // the destructor frees them a second time.

    while (root_ != nullptr)
	    remove(root_->rec.id);
}

std::pair<MemoryIndex::Node *, MemoryIndex::Node *> MemoryIndex::find_internal(std::uint64_t id) const {
	Node *n, *p = nullptr;

	for (n = root_; n != nullptr && n->rec.id != id;) {
		p = n;
		n = n->rec.id < id ? n->right : n->left;
	}

	return { p, n };
}

std::size_t MemoryIndex::height_of(const Node *n) {
	if (n == nullptr)
		return 0;

	return std::max(height_of(n->left), height_of(n->right)) + 1;
}

void MemoryIndex::for_each_in_order_recursive(Node *n, const std::function<void(const MemoryRecord&)>& fn) const {
	if (n->left != nullptr)
		for_each_in_order_recursive(n->left, fn);
	fn(n->rec);
	if (n->right != nullptr)
		for_each_in_order_recursive(n->right, fn);
}
