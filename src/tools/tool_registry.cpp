// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Every function below is a stub so the starter compiles. Replace each body.

#include "tools/tool_registry.h"

ToolRegistry::ToolRegistry(): num_buckets(16), num_keys(0), max_chain_len(0) {
	buckets = new Bucket[num_buckets]();
}

ToolRegistry::~ToolRegistry() {
	for (std::size_t i = 0; i < bucket_count(); i++) {
		if (buckets[i].tool == nullptr)
			continue;

		for (Bucket *b = buckets[i].next, *next; b != nullptr; b = next) {
			next = b->next;
			delete b;
		}
	}

	delete[] buckets;
}

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
	std::uint64_t h = 0x14650fb0739d0383;

	/* FNV-1a */
	for (unsigned char c : key) {
		h ^= c;
		h *= 0x100000001b3;
	}

	return h;
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
	std::size_t index, chain_len = 1;
	Bucket *b;

	if (tool == nullptr)
		return false;

	if (find(tool->name()) != nullptr) {
		/* This should automatically free tool. */
		return false;
	}

	if (load_factor() >= 0.875) {
		std::size_t num_buckets_old = num_buckets;
		struct Bucket *buckets_old = buckets;

		num_buckets *= 2;
		num_keys = 0;
		max_chain_len = 0;
		buckets = new Bucket[num_buckets]();

		for (std::size_t i = 0; i < num_buckets_old; i++) {
			struct Bucket *b;

			if (buckets_old[i].tool == nullptr)
				continue;

			add(std::move(buckets_old[i].tool));
			for (b = buckets_old[i].next; b != nullptr;) {
				Bucket *tmp = b->next;

				add(std::move(b->tool));

				delete b;
				b = tmp;
			}
		}

		delete[] buckets_old;
	}

	num_keys++;

	/*
	 * Equivalent to hash() % bucket_count() since bucket_count()
	 * is a power of 2.
	 */
	index = hash(tool->name()) & (bucket_count() - 1);
	if (buckets[index].tool == nullptr) {
		buckets[index].tool = std::move(tool);
		return true;
	}

	b = &buckets[index];
	while (b->next != nullptr) {
		b = b->next;
		chain_len++;
	}

	/*
	 * Do this here instead of looping over the whole
	 * table in max_probe_length().
	 */
	if (chain_len > max_chain_len)
		max_chain_len = chain_len;

	b->next = new Bucket();
	b = b->next;

	b->tool = std::move(tool);
	return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
	std::size_t index;

	/*
	 * Equivalent to hash() % bucket_count() since bucket_count()
	 * is a power of 2.
	 */
	index = hash(name) & (bucket_count() - 1);
	if (buckets[index].tool == nullptr)
		return nullptr;

	for (Bucket *b = &buckets[index]; b != nullptr; b = b->next) {
		if (b->tool->name() == name)
			return b->tool.get();
	}

	return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
	return num_keys;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
	for (std::size_t i = 0; i < bucket_count(); i++) {
		if (buckets[i].tool == nullptr)
			continue;

		for (Bucket *b = &buckets[i]; b != nullptr; b = b->next)
			fn(*b->tool);
	}
}

std::size_t ToolRegistry::bucket_count() const noexcept {
	return num_buckets;
}

double ToolRegistry::load_factor() const noexcept {
	return (double)size() / bucket_count();
}

std::size_t ToolRegistry::max_probe_length() const {
	return max_chain_len;
}
