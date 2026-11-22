// src/skills/dep_graph.cpp — YOU IMPLEMENT. Spec §6.2.

#include "skills/dep_graph.h"

bool DepGraph::add_node(const std::string& name) {
    std::vector<std::string> req_by;

    if (requires_.find(name) != requires_.end())
	    return false;

    requires_.insert({name, {}});
    if (required_by_.find(name) == required_by_.end())
	    required_by_.insert({name, {}});
    return true;
}

bool DepGraph::add_edge(const std::string& from, const std::string& to) {
    auto search = requires_.find(from);

    if (search == requires_.end())
	    return false;

    for (std::string e : search->second) {
	    if (e == to)
		    return false; /* edge exists. */
    }
    search->second.push_back(to);

    /* Add to required_by_. */
    if ((search = required_by_.find(to)) != required_by_.end())
	    search->second.push_back(from);
    else
	    required_by_.insert({to, {from}});

    return true;
}

bool DepGraph::has_node(const std::string& name) const {
    return requires_.find(name) != requires_.end();
}

std::size_t DepGraph::node_count() const noexcept {
	return requires_.size();
}

const std::vector<std::string>& DepGraph::requirements(const std::string& name) const {
    // return the stored list; for an unknown name return a reference
    // to an empty vector that outlives the call (e.g. a function-local
    // static const).
    static const std::vector<std::string> kNone;
    auto search = requires_.find(name);

    return search == requires_.end() ? kNone : search->second;
}

bool DepGraph::order_from(const std::string& root, std::vector<std::string>* out,
                          std::string* error) const {
    // DFS with three colors (white / gray = on the current path /
    // black = finished). Spec §6.2 gives the exact error strings, and
    // Appendix D traces the diamond.
    // Clues:
    //  * Keep a vector<string> path of the gray nodes: push_back when a node
    //    turns gray, pop_back when it turns black. When an edge reaches a
    //    gray node x, the cycle is the part of path from x to the end, then
    //    x again. Check yourself with a>b, a>c, c>a: the answer is
    //    "cycle: a -> c -> a" (b is already finished and must not appear).
    //  * Append a node to *out when it turns BLACK (after its requirements),
    //    never when it turns gray.
    //  * Order of checks for each edge: missing node, then gray (cycle),
    //    then white (visit). Black: skip, that is not a cycle.
    //  * Separators are exactly " -> " (space, dash, greater-than, space).
    auto search = requires_.find(root);
    const std::vector<std::string> *refs, fake = {root};
    std::vector<std::pair<decltype(refs), decltype(refs->begin())>> saved;
    std::unordered_set<std::string> visited;

    if (search == requires_.end()) {
	    *error = root;
	    error->insert(0, "unknown: ");
	    return false;
    }

    *out = {};
    refs = &search->second;

    /*
     * Push a fake node that points to root.
     * This is a bit of a hack.
     */
    saved.push_back({&fake, fake.begin()});
    for (auto r = refs->begin();;) {
	    if (r == refs->end()) {
		    if (saved.empty())
			    break;

		    auto res = saved.back();

		    saved.pop_back();
		    refs = res.first;
		    r = res.second;

		    out->push_back(*r);
		    r++;

		    /*
		     * If this was also the last node for the parent,
		     * go back to the top.
		     */
		    if (r == refs->end())
			continue;
	    }

	    /* Check if this is on the current path. */
	    for (auto n = saved.begin(); n != saved.end(); n++) {
		    if (*r != *n->second)
			    continue;

		    *error = "cycle: ";
		    for (; n != saved.end(); n++)
			    *error += *n->second + " -> ";
		    *error += *r;
		    return false;
	    }

	    /* We now know it's not on the current path, check if we've visited it before. */
	    if (visited.find(*r) != visited.end()) {
		    r++;
		    continue;
	    }

	    visited.insert(*r);
	    if ((search = requires_.find(*r)) == requires_.end()) {
		    *error = "missing: ";
		    *error += *r + " (required by " + *saved.back().second + ')';
		    return false;
	    }

    	    /* Save the reference list for the node, and where we stopped. */
	    saved.push_back({refs, r});
	    refs = &search->second;
	    r = refs->begin();
    }

    *error = "";
    return true;
}

std::vector<std::string> DepGraph::dependents(const std::string& name) const {
    // BFS over the reversed edges.
    // Clues:
    //  * Mark `name` itself as seen before you start, so a cycle can't add
    //    it to the result.
    //  * The order is by (distance, name). Easiest: sort each BFS level
    //    alphabetically before appending it. Sorting the whole result by
    //    name alone loses the nearest-first order.
    //  * std::queue<std::pair<std::string, int>> (node, distance) works, or
    //    process one level (a vector) at a time.
    auto search = required_by_.find(name);
    std::vector<std::string> deps, save;
    std::unordered_set<std::string> visited;

    if (search == required_by_.end())
	    return deps;

    visited.insert(name);
    for (std::vector<std::string> dv = search->second;;) {
	    /* Sort alphabetically. */
	    std::sort(dv.begin(), dv.end());

	    for (std::string d : dv) {
		    if (visited.find(d) != visited.end())
			    continue;
	
		    deps.push_back(d);
		    visited.insert(d);

		    /* Add any nodes that depend on this one. */
		    search = required_by_.find(d);
		    save.insert(save.end(), search->second.begin(), search->second.end());
	    }

	    if (save.empty())
		    return deps;

	    dv = save;
	    save = {};
    }

    /* Not reached. */
}
