#include <string>
#include <iostream>
#include <cassert>

#include <core/sentinel_scanner.h>

SentinelScanner::SentinelScanner(std::string sentinel) : sentinel_(sentinel), pending_("") {
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
	std::size_t i, j;
	Out ret;

	for (i = pending_.length(), j = 0; i < sentinel_.length() && j < chunk.length(); i++, j++) {
		if (sentinel_[i] != chunk[j])
			break;
	}

	if (i == sentinel_.length()) {
		/*
		 * The rest of the chunk matched the
		 * sentinel. Stop here.
		 */

		/* Move anything leftover in the chunk to safe_text. */
		ret.safe_text = std::string(j < chunk.length() ? chunk.substr(j, chunk.length() - j) : "");
		ret.sentinel_found = true;
	} else if (j < chunk.length()) {
		std::string safe_text;
		/*
		 * No sentinel was found at the begining of the chunk. The rest of
		 * the chunk may have one though.
		 */
		safe_text = pending_ + std::string(chunk.substr(0, j + 1));
		pending_ = "";

		ret = feed(chunk.substr(j + 1, chunk.length() - (j + 1)));
		ret.safe_text = safe_text + ret.safe_text;
	} else {
		/* Chunk was too short, put it in pending_. */
		ret.safe_text = "";
		ret.sentinel_found = false;

		pending_ += std::string(chunk);

		/* Sanity check. */
		assert(pending_.length() < sentinel_.length());
	}

	return ret;
}

SentinelScanner::Out SentinelScanner::flush() {
	return Out(pending_, false);
}
