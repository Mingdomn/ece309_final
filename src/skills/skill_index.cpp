// src/skills/skill_index.cpp — YOU IMPLEMENT. Spec §6.3.

#include "skills/skill_index.h"
#include "util/min_heap.h"
#include "util/text.h"

void SkillIndex::add(std::size_t doc_id, std::string_view text) {
    // Clue: use unique_tokens(text), not tokenize(text): a word that appears
    // twice in one document must post its doc_id only once.

    for (auto t : unique_tokens(text)) {
    	auto search = postings_.find(t);

        if (search == postings_.end())
    	    postings_.insert({t, {doc_id}});
        else
    	    search->second.push_back(doc_id);
    }

}

const std::vector<std::size_t>* SkillIndex::postings(std::string_view word) const {
    // Clue: the map's keys are std::string, so find(word) with a
    // string_view does not compile in C++17: use find(std::string(word)).
    // Return the address of the vector inside the map (&it->second).
    auto search = postings_.find(std::string(word));

    if (search == postings_.end())
	    return nullptr;

    return &search->second;
}

std::size_t SkillIndex::word_count() const noexcept {
    return postings_.size();
}

std::vector<std::size_t> SkillIndex::search(std::string_view query, std::size_t k) const {
    // score documents using only the posting lists of the query's
    // words, then rank with top_k().
    // Clues:
    //  * Count with  std::unordered_map<std::size_t, std::size_t> score;
    //    walking the posting list of each word of unique_tokens(query)
    //    (a repeated query word counts once).
    //  * Put (score, doc_id) pairs in a vector and call top_k with a
    //    comparator meaning "a is WORSE than b": lower score is worse; with
    //    equal scores the LARGER doc_id is worse.
    //  * Don't copy the lambda in skill_index_linear.cpp: that one is a
    //    std::sort order ("a comes first"), the opposite direction.
    std::unordered_map<std::size_t, std::size_t> score;
    std::vector<std::pair<std::size_t, std::size_t>> score_vec;
    std::vector<std::size_t> ret;

    for (auto t : unique_tokens(query)) {
	    auto p = postings(t);

	    if (p == nullptr)
		    continue;

	    for (std::size_t id : *p)
		    score[id]++;
    }

    for (auto s : score)
	    score_vec.push_back(s);

    score_vec = top_k<std::pair<std::size_t, std::size_t>, struct compare>(score_vec, k);
    for (auto s : score_vec) {
	    if (s.second >= 1)
	    	ret.push_back(s.first);
    }

    return ret;
}

bool SkillIndex::compare::operator()(const std::pair<std::size_t, std::size_t> &lhs,
    const std::pair<std::size_t, std::size_t> &rhs) const {
    if (lhs.second < rhs.second)
	    return true;
    else if (lhs.second > rhs.second)
	    return false;
    else
	    return lhs.first > rhs.first ? true : false;
}
