#pragma once

class SentinelScanner {
private:
	const std::string sentinel_;
	std::string pending_;
public:
	struct Out {
		std::string safe_text;
		bool sentinel_found;

		Out(std::string str, bool found) {
			safe_text = str;
			sentinel_found = found;
		}

		Out() {}
	};

	explicit SentinelScanner(std::string sentinel);

	Out feed(std::string_view chunk);

	Out flush();
};
