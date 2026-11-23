// tests/final/test_final.cpp — YOUR TEST SUITE (spec §8).
//
// You need at least 20 tests. The 8 examples below are given and count:
//   Memory  (4): heap, lru, bst, ctx
//   Policy  (2): pol x2
//   Skills  (2): graph, index
// Write at least 12 more of your own, covering the list in spec §8.
// Name tests <area>_<what> with the areas heap, lru, bst, ctx, pol, graph,
// index, so the summary groups them.
//
//   ./build/test_final              run everything
//   ./build/test_final lru_         only tests whose names start with lru_
//   ./build/test_final =lru_evicts_least_recently_used    one test, exactly
//   ./build/test_final --list       list test names
//
// If a sanitizer error (ASan/UBSan) aborts the run, the tests after it do
// not run: rerun the failing test alone with =its_name and read the FIRST
// stack frame that is in your own file.
//
// CHECK_EQ(a, b) prints both values when it fails (vectors too), so prefer
// it over CHECK(a == b).
//
// Helpers you can use (tests/test_helpers.h): TempDir, write_text,
// write_skill, FixedPolicy, ScriptedConfirmer, LinesInput, StringOutput,
// parse_ok, args1. ManualClock is in policy/policy.h.

#include "../mini_test.h"
#include "../test_helpers.h"

#include "memory/context_builder.h"
#include "memory/memory_index.h"
#include "memory/memory_store.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/dep_graph.h"
#include "skills/skill_catalog.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"
#include "util/min_heap.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// =========================================================================
// Memory — 4 examples
// =========================================================================

// Example 1 (heap): items come back smallest first, duplicates included.
TEST(heap_pops_in_order) {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 1, 3}) h.push(x);
    CHECK_EQ(h.size(), std::size_t{5});
    CHECK_EQ(h.top(), 1);
    std::vector<int> got;
    while (!h.empty()) got.push_back(h.pop());
    CHECK_EQ(got, (std::vector<int>{1, 1, 3, 4, 5}));
}

TEST(heap_empty_pop_throw) {
    bool threw = false;
    MinHeap<int> h;

    try {
	    h.top();
    } catch (const std::out_of_range &e) {
	    threw = true;
    }

    CHECK(threw);
    
    threw = false;
    try {
	    h.pop();
    } catch (const std::out_of_range &e) {
	    threw = true;
    }

    CHECK(threw);
}

TEST(heap_topk_k) {
    std::vector<int> data = {0, 1, 2, 3, 4};

    /* Check with k = 0. */
    CHECK_EQ(top_k(data, 0), (std::vector<int>()));

    /* Check with k > n. */
    CHECK_EQ(top_k(data, data.size() + 50), (std::vector<int>{4, 3, 2, 1, 0}));
}

/* 10,000 random integers. */
TEST(heap_topk_random) {
    std::vector<int> data, ans;

    std::srand(std::time(nullptr));
    for (std::size_t i = 0; i < 10000; i++)
	    data.push_back(std::rand());

    ans = top_k(data, data.size());

    /* Sort "data", then compare with top_k. */
    std::sort(data.begin(), data.end(), std::greater());
    CHECK_EQ(ans, data);
}

/* Similar to the above test, but use 100,000 numbers with k = 10, and profile. */
TEST(heap_topk_random_profile) {
    std::vector<int> data, ans;
    double topk_time, sort_time;

    std::srand(std::time(nullptr));
    for (std::size_t i = 0; i < 100000; i++)
	    data.push_back(std::rand());

    auto f_start = std::chrono::high_resolution_clock::now();
    ans = top_k(data, 10);
    topk_time = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - f_start).count();

    /* Sort "data", then compare with top_k. */
    f_start = std::chrono::high_resolution_clock::now();
    std::sort(data.begin(), data.end(), std::greater());
    sort_time = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - f_start).count();

    std::printf("%s: top_k(k = 10): %fms, std::sort: %fms\n", __func__, topk_time, sort_time);

    CHECK(topk_time < sort_time);
}

// Example 2 (lru): the least recently USED entry is evicted, and get()
// counts as a use.
TEST(lru_evicts_least_recently_used) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    REQUIRE(c.get("a") != nullptr);   // "a" is now the most recent
    c.put("c", 3);                    // full: "b" must go
    CHECK(c.contains("a"));
    CHECK(!c.contains("b"));
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"c", "a"}));
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

/* Test updating an existing node while cache is full. */
TEST(lru_existing_update) {
    LruCache<std::string, int> c(2);
    decltype(c.stats()) stats;

    c.put("a", 1);
    c.put("b", 2);
    c.put("a", 3);

    /* Check that the last put() made "a" the most recent. */
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"a", "b"}));

    /* Check that the value was updated. */
    CHECK_EQ(*c.get("a"), 3);

    /* One hit, no evictions, no misses. */
    stats = c.stats();
    CHECK_EQ(stats.hits, std::size_t(1));
    CHECK_EQ(stats.evictions, std::size_t(0));
    CHECK_EQ(stats.misses, std::size_t(0));
}

/* Test when capacity = 0, and 1. */
TEST(lru_capacity) {
    LruCache<char, int> c0(0), c1(1);

    /* Should do nothing. */
    c0.put('a', 1);
    CHECK_EQ(c0.get('a'), nullptr);

    /* Make sure the miss is counted. */
    CHECK_EQ(c0.stats().misses, std::size_t(1));

    /* Should have no nodes. */
    CHECK_EQ(c0.keys_mru_to_lru().size(), std::size_t(0));

    c1.put('a', 1);
    REQUIRE(c1.get('a'));

    /* Should evict a. */
    c1.put('b', 2);
    CHECK_EQ(c1.get('a'), nullptr);

    /* Make sure the eviction is counted. */
    CHECK_EQ(c1.stats().evictions, std::size_t(1));

    /* Make sure there's only one node. */
    CHECK_EQ(c1.keys_mru_to_lru().size(), std::size_t(1));
}

/* Check the erasure of nodes at different places. */
TEST(lru_erase) {
    LruCache<char, int> c(4);

    c.put('a', 0);
    c.put('b', 1);
    c.put('c', 2);
    c.put('d', 3);

    REQUIRE(c.keys_mru_to_lru() == (std::vector<char>{'d', 'c', 'b', 'a'}));

    /* Erase head. */
    c.erase('d');
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<char>{'c', 'b', 'a'}));

    /* Erase middle node. */
    c.erase('b');
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<char>{'c', 'a'}));

    /* Erase tail. */
    c.erase('a');
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<char>{'c'}));
}

/* These were brefily checkd in other tests, do it more thoroughly here. */
TEST(lru_stats) {
    LruCache<char, int> c(3);
    decltype(c.stats()) stats;

    c.put('a', 0);
    c.put('b', 1);
    c.put('c', 2);

    /* Should be zero. */
    stats = c.stats();
    CHECK_EQ(stats.hits, 0ull);
    CHECK_EQ(stats.misses, 0ull);
    CHECK_EQ(stats.evictions, 0ull);

    REQUIRE(c.get('a'));

    /* One hit. */
    stats = c.stats();
    CHECK_EQ(stats.hits, 1ull);
    CHECK_EQ(stats.misses, 0ull);
    CHECK_EQ(stats.evictions, 0ull);

    /* b should get evicted. */
    c.put('d', 3);

    /* One eviction. */
    stats = c.stats();
    CHECK_EQ(stats.hits, 1ull);
    CHECK_EQ(stats.misses, 0ull);
    CHECK_EQ(stats.evictions, 1ull);

    /* Make sure it actually was. */
    REQUIRE(c.keys_mru_to_lru() == (std::vector<char>{'d', 'a', 'c'}));

    REQUIRE(!c.get('e'));

    /* One miss. */
    stats = c.stats();
    CHECK_EQ(stats.hits, 1ull);
    CHECK_EQ(stats.misses, 1ull);
    CHECK_EQ(stats.evictions, 1ull);

    /* erase() shouldn't change stats. */
    c.erase('c');

    stats = c.stats();
    CHECK_EQ(stats.hits, 1ull);
    CHECK_EQ(stats.misses, 1ull);
    CHECK_EQ(stats.evictions, 1ull);

    /* Sanity check for final value. */
    REQUIRE(c.keys_mru_to_lru() == (std::vector<char>{'d', 'a'}));
}

// Example 3 (bst): removing a node with two children keeps the tree sorted,
// keeps every other record, and lowers size() by exactly one.
TEST(bst_remove_two_children) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 60, 80}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    REQUIRE(t.remove(70));
    std::vector<std::uint64_t> ids;
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 50, 60, 80}));
    CHECK(t.find(70) == nullptr);
    REQUIRE(t.find(80) != nullptr);
    CHECK_EQ(t.find(80)->text, std::string("m80"));
    CHECK_EQ(t.size(), std::size_t{4});
}

/* Traversal on empty tree. */
TEST(bst_in_order_empty) {
    MemoryIndex t;

    t.for_each_in_order([&](const MemoryRecord &r) { (void)r; CHECK(false); });
}

/*
 * Test the following remove cases:
 * Remove root, remove a leaf, remove a node with one child.
 */
TEST(bst_remove_cases) {
    MemoryIndex t;
    std::vector<std::uint64_t> ids;

    for (std::uint64_t id : {50, 30, 70, 60, 80, 75}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));

    /* Remove root. */
    REQUIRE(t.remove(50));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 60, 70, 75, 80}));
    ids = {};

    /* Remove leaf. */
    REQUIRE(t.remove(30));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{60, 70, 75, 80}));
    ids = {};

    /* Remove node with one left child. */
    REQUIRE(t.remove(80));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{60, 70, 75}));
    ids = {};

    /* Remove node with one right child. */
    REQUIRE(t.remove(70));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{60, 75}));
    ids = {};

    /* Remove node where successor has a right child. */
    REQUIRE(t.insert(MemoryRecord{65, "m65"}));
    REQUIRE(t.remove(75));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{60, 65}));
}

TEST(bst_duplicate_insert) {
    MemoryIndex t;

    for (std::uint64_t id : {50, 30, 70, 60, 80, 75}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));

    /* Should be rejected. */
    CHECK(!t.insert(MemoryRecord{30, ""}));
}

/* Check in order traversal with full tree. */
TEST(bst_in_order) {
    MemoryIndex t;
    std::vector<std::uint64_t> ids;

    for (std::uint64_t id : {100, 70, 50, 40, 55, 75, 71, 76, 110, 115, 120, 111, 105, 106, 104}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });

    CHECK_EQ(ids, (std::vector<std::uint64_t>{40, 50, 55, 70, 71, 75, 76, 100, 104, 105, 106, 110, 111, 115, 120}));
}

TEST(bst_height) {
    MemoryIndex t;
    std::vector<std::uint64_t> ids;

    /* Zero for empty tree. */
    CHECK_EQ(t.height(), 0ull);

    /* One for a single node. */
    REQUIRE(t.insert(MemoryRecord{0, "m0"}));
    CHECK_EQ(t.height(), 1ull);

    t.clear();

    /* Same tree as above. */
    for (std::uint64_t id : {100, 70, 50, 40, 55, 75, 71, 76, 110, 115, 120, 111, 105, 106, 104}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });

    CHECK_EQ(t.height(), 4ull);
}

TEST(bst_height_stress) {
    MemoryIndex t;
    std::vector<std::uint64_t> ids;
    std::size_t seq_height, rand_height;

    /* Implement our own generator that actually just uses std::rand. */
    /* We don't need anything too fancy. */
    struct shuffle_gen {
	    using result_type = unsigned;
	    static constexpr result_type max() {
		    return RAND_MAX;
	    }
	    static constexpr result_type min() {
		    return 0;
	    }
	    shuffle_gen() {
		    std::srand(std::time(nullptr));
	    }
	    result_type operator()() {
		    return std::rand();
	    }
    };

    /* sequential */
    for (std::uint64_t id = 1; id <= 1000; id++) {
	    REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
	    ids.push_back(id);
    }

    std::printf("%s: Sequential height: %ld, ", __func__, (seq_height = t.height()));
    CHECK_EQ(seq_height, 1000ull);

    t.clear();
    std::shuffle(ids.begin(), ids.end(), shuffle_gen());

    /* random */
    for (std::uint64_t id : ids)
	    REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));

    std::printf("Shuffled height: %ld\n", (rand_height = t.height()));

    /*
     * Technically, this can fail.
     *
     * If it does, maybe I should have been mining bitcoin
     * instead of working on this...
     */
    CHECK(rand_height < seq_height);
}

// Example 4 (ctx): the worked example in spec §4.4.
TEST(ctx_spec_example) {
    std::vector<ContextItem> items{
        {Role::System, 10, false},     // 0  protected
        {Role::User, 10, false},       // 1
        {Role::Assistant, 10, false},  // 2  dropped 3rd (oldest assistant)
        {Role::Tool, 10, false},       // 3  dropped 1st
        {Role::Assistant, 10, false},  // 4
        {Role::Tool, 10, false},       // 5  dropped 2nd
        {Role::User, 10, false},       // 6  recent: protected
        {Role::Assistant, 10, false}}; // 7  recent: protected
    ContextPlan p = select_context(items, 50, 2);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 4, 6, 7}));
    CHECK_EQ(p.total_tokens, std::size_t{50});
    CHECK(!p.over_budget);
}

TEST(ctx_protected_survive) {
    std::vector<ContextItem> items{
	    {Role::System, 10, false}, /* system */
	    {Role::User, 10, true}, /* pinned */
	    {Role::Tool, 10, true}, /* pinned */
	    {Role::Assistant, 10, true}, /* pinned */
	    {Role::User, 10, false}, /* NOT protected. */
	    {Role::User, 10, false} /* recent */
    };
    ContextPlan p = select_context(items, 40, 1);

    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 2, 3, 5}));
    CHECK_EQ(p.total_tokens, 50ull);

    /* Also check over_budget. */
    CHECK(p.over_budget);
}

TEST(ctx_drop_order) {
    std::vector<ContextItem> items{
	    {Role::System, 10, false}, /* system */
	    {Role::User, 10, false},
	    {Role::Assistant, 10, false},
	    {Role::Tool, 10, false},
    };
    ContextPlan p = select_context(items, 30, 0);

    /* Tool should go first. */
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 2}));

    /* Protect Tool. */
    p = select_context(items, 30, 1);

    /* Assistant should go second. */
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 3}));

    /* Protect Assistant. */
    p = select_context(items, 30, 2);

    /* User should go last. */
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 2, 3}));

    items = {
	    {Role::User, 10, false},
	    {Role::Assistant, 10, false},
	    {Role::Assistant, 10, false},
	    {Role::Tool, 10, false}
    };

    /*
     * Tool is dropped first, then older assistant.
     */
    p = select_context(items, 20, 0);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 2}));
}

// =========================================================================
// Policy — 2 examples
// =========================================================================

// Example 5 (pol): the sliding window. A request exactly `window` seconds
// after an allowed one no longer counts it.
TEST(pol_rate_limit_window_boundary) {
    ManualClock clock(100);
    RateLimitPolicy p(2, 10.0, clock);   // at most 2 per 10 s
    CommandRequest req;
    req.program = "ls";
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 100
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 101
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Deny);    // t = 102: two in the window
    clock.advance(8);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 110: 100 has left (110 - 100 = 10)
    CHECK(p.tracked() <= 2u);
}

// Example 6 (pol): first Deny wins and later policies are not asked; with no
// Deny, the FIRST Ask is returned unchanged.
TEST(pol_chain_first_deny_stops_first_ask_wins) {
    auto ask1 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask1");
    auto ask2 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask2");
    PolicyChain chain;
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "ok")));
    REQUIRE(chain.add(std::move(ask1)));
    REQUIRE(chain.add(std::move(ask2)));
    CommandRequest req;
    req.program = "git";
    Decision d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Ask);
    CHECK_EQ(d.policy, std::string("ask1"));

    auto deny = std::make_unique<FixedPolicy>(Decision::Kind::Deny, "no");
    auto after = std::make_unique<FixedPolicy>(Decision::Kind::Allow, "after");
    FixedPolicy* after_ptr = after.get();   // still valid: the chain owns it now
    REQUIRE(chain.add(std::move(deny)));
    REQUIRE(chain.add(std::move(after)));
    d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Deny);
    CHECK_EQ(d.policy, std::string("no"));
    CHECK_EQ(after_ptr->calls, 0);     // never evaluated: the chain stopped at the Deny
}

TEST(pol_allow_list) {
    AllowlistPolicy p({"git", "less", "tail", "head", "tar"});
    CommandRequest req;

    /* Why is the LLM peeking at my emails? */
    req.program = "aerc";
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Deny);

    req.program = "tar";
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Allow);
}

TEST(pol_pattern) {
    /* Note that the empty pattern should be a nop. */
    ArgumentPatternPolicy p({"rm -rf", "sudo", "doas", "su", ":(){:|:&};:", ""});
    CommandRequest req;

    req.program = "rm";
    req.args = {"-rf", "/"};
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Deny);

    req.args = { "-v", "/tmp/workdir/somefile" };
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Allow);

    req.program.insert(0, "doas ");
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Deny);

    req.program = "sh";
    req.args = {"-c", ":(){:|:&};:"};
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Deny);

    req.args = {"-c", "totally_innocent_script.sh"};
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Allow);
}

TEST(pol_ask) {
    ConfirmPolicy p({"rm", "pkg"});
    CommandRequest req;

    req.program = "rm";
    req.args = { "-v", "/tmp/workdir/somefile" };
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Ask);

    req.program = "cp";
    req.args = { "-vr", "/tmp/workdir/somefile1", "/tmp/workdir/somefile2" };
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Allow);

    req.program = "pkg";
    req.args = {"install", "drm-kmod"};
    CHECK_EQ(p.evaluate(req).kind, Decision::Kind::Ask);
}

TEST(pol_nested_chain) {
    CommandRequest req;
    PolicyChain chain;
    std::unique_ptr<PolicyChain> internal_chain = std::make_unique<PolicyChain>();

    internal_chain->add(std::make_unique<ConfirmPolicy>(std::vector<std::string>{"rm", "pkg"}));
    internal_chain->add(std::make_unique<ArgumentPatternPolicy>(std::vector<std::string>{"rm -rf"}));

    chain.add(std::make_unique<AllowlistPolicy>(std::vector<std::string>{"rm", "pkg", "tail"}));
    chain.add(std::move(internal_chain));

    req.program = "rm";
    req.args = { "-v", "/tmp/workdir/somefile" };
    CHECK_EQ(chain.evaluate(req).kind, Decision::Kind::Ask);

    /* Not in allowlist */
    req.program = "ls";
    CHECK_EQ(chain.evaluate(req).kind, Decision::Kind::Deny);

    /* Caught by ArgumentPatternPolicy in the child chain. */
    req.program = "rm";
    req.args = { "-rf", "/" };
    CHECK_EQ(chain.evaluate(req).kind, Decision::Kind::Deny);

    /* In allowlist, not blocked by anything else. */
    req.program = "tail";
    req.args = { "/tmp/workdir/log" };
    CHECK_EQ(chain.evaluate(req).kind, Decision::Kind::Allow);
}

/* Test empty chain. */
TEST(pol_empty_chain) {
    PolicyChain chain;
    CommandRequest req;

    req.program = "top";
    CHECK_EQ(chain.evaluate(req).kind, Decision::Kind::Deny);
}

// =========================================================================
// Skills — 2 examples
// =========================================================================

TEST(graph_disallow_duplicate_nodes) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) {
	    CHECK(g.add_node(n));
	    CHECK(!g.add_node(n));
    }
}

TEST(graph_disallow_duplicate_edges) {
    DepGraph g;
    const std::vector<const char *> nodes = {"a", "b", "c", "d"};
    for (const char* n : nodes) g.add_node(n);
    for (auto n1 : nodes) {
	    for (auto n2 : nodes) {
		    CHECK(g.add_edge(n1, n2));
		    CHECK(!g.add_edge(n1, n2));
	    }
    }
}

// Example 7 (graph): post-order load order, and a cycle message that lists
// only the nodes still on the DFS path (b is already finished).
TEST(graph_order_and_cycle_message) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) g.add_node(n);
    g.add_edge("a", "b");   // a requires b
    g.add_edge("a", "c");
    g.add_edge("b", "d");
    g.add_edge("c", "d");   // diamond: d is reached twice, but it is not a cycle
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"d", "b", "c", "a"}));

    g.add_edge("c", "a");   // now a -> c -> a is a cycle
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> c -> a"));
}

/* Test a cycle that doesn't include the root. */
TEST(graph_cycle_no_root) {
    DepGraph g;
    std::vector<std::string> order;
    std::string error;

    for (const char* n : {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l"}) g.add_node(n);
    g.add_edge("a", "b");
    g.add_edge("b", "c");
    g.add_edge("b", "d");
    g.add_edge("a", "e");
    g.add_edge("e", "f");
    g.add_edge("f", "j");
    g.add_edge("j", "g");
    g.add_edge("j", "h");
    g.add_edge("j", "i");
    g.add_edge("j", "k");
    g.add_edge("k", "a"); /* closes the cycle. */
    g.add_edge("l", "a");
    CHECK(!g.order_from("l", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> e -> f -> j -> k -> a"));
}

TEST(graph_self_loop) {
    DepGraph g;
    std::vector<std::string> order;
    std::string error;

    for (const char* n : {"a", "b", "c"}) g.add_node(n);
    g.add_edge("a", "b");
    g.add_edge("b", "c");
    g.add_edge("c", "c");
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: c -> c"));
}

TEST(graph_missing_req) {
    DepGraph g;
    std::vector<std::string> order;
    std::string error;

    for (const char* n : {"a", "b"}) g.add_node(n);
    g.add_edge("a", "b");
    g.add_edge("b", "c");
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("missing: c (required by b)"));
}

TEST(graph_dependent_order) {
    DepGraph g;
    std::vector<std::string> deps;

    for (const char* n : {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l"}) g.add_node(n);
    g.add_edge("a", "b");
    g.add_edge("b", "c");
    g.add_edge("b", "d");
    g.add_edge("a", "e");
    g.add_edge("e", "f");
    g.add_edge("f", "j");
    g.add_edge("j", "g");
    g.add_edge("j", "h");
    g.add_edge("j", "i");
    g.add_edge("j", "k");
    g.add_edge("k", "a");
    g.add_edge("l", "a");
    deps = g.dependents("a");
    CHECK_EQ(deps, (std::vector<std::string>{"k", "l", "j", "f", "e"}));
}

/* Stress test with 1000 nodes. */
TEST(graph_stress) {
    DepGraph g;
    std::vector<std::string> check, ans;
    std::string error;
    const std::size_t max = 1000;

    g.add_node("0");
    ans.push_back("0");
    for (std::size_t i = 1; i < max; i++) {
	    g.add_node(std::to_string(i));
	    /* Each node connected to the one behind it. */
	    g.add_edge(std::to_string(i), std::to_string(i - 1));

	    ans.push_back(std::to_string(i));
    }

    CHECK(g.order_from(std::to_string(max - 1), &check, &error));
    CHECK_EQ(check, ans);

    ans.erase(ans.begin());
    check = g.dependents("0");
    CHECK_EQ(check, ans);
}

// Example 8 (index): rank by number of distinct query words; ties go to the
// smaller doc id.
TEST(index_ranking_and_ties) {
    SkillIndex ix;
    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");
    CHECK_EQ(ix.search("csv table numbers", 10), (std::vector<std::size_t>{1, 0, 2}));
    CHECK_EQ(ix.search("format table", 1), (std::vector<std::size_t>{2}));
    REQUIRE(ix.postings("numbers") != nullptr);
    CHECK_EQ(*ix.postings("numbers"), (std::vector<std::size_t>{0, 1}));
}

TEST(index_unknown_word) {
    SkillIndex ix;

    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");

    CHECK_EQ(ix.search("unknown", 10), std::vector<std::size_t>());
}

TEST(index_profile) {
    std::vector<std::size_t> sizes = {100, 1000, 5000};
    std::vector<const char *> words = { "the", "quick", "brown", "fox", "jumped", "over", "the", "lazy", "dog" };

    std::srand(std::time(nullptr));

    for (std::size_t size : sizes) {
        SkillIndex ix;
        double my_search = 0, linear_time = 0;
	std::vector<std::string> docs(size);

        for (std::size_t i = 0; i < size; i++) {
    	    /* Insert a random number of random words in each doc. */
    	    unsigned nwords = std::rand() % 8 + 1;
    	    for (unsigned j = 0; j < nwords; j++) {
		    const char *word = words[std::rand() % 9];

		    docs[i] += std::string(word) + ' ';
	    }

    	    ix.add(i, docs[i]);
        }
    
	/* Loop up some random phrases. */
	for (std::string w : { "the", "the quick", "brown fox over", "lazy dog", "jumped fox", "brown quick lazy" }) {
            auto f_start = std::chrono::steady_clock::now();
	    std::vector<std::size_t> s1, s2;

    	    s1 = ix.search(w, size);
            my_search += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - f_start).count();

	    f_start = std::chrono::steady_clock::now();
	    s2 = linear_search(docs, w, size);
            linear_time += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - f_start).count();

	    /* Sanity check. */
	    CHECK_EQ(s1, s2);
	}
    
        /* Average across searches. */
        my_search /= 6;

        std::printf("%s: my_search: %fms (size = %zu)\n", __func__, my_search, size);

        /* Average across searches. */
        linear_time /= 6;

        std::printf("%s: linear_search: %fms (size = %zu)\n", __func__, linear_time, size);
    }
}

// =========================================================================
// Your tests (at least 12 more). Spec §8 lists what they must cover.
// =========================================================================

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
