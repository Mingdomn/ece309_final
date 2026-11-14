// src/tools/workspace.cpp — YOU IMPLEMENT (spec §4.4).
//
// Workspace::resolve: a purely lexical check, steps 1–6 of the spec.
// Do not touch the filesystem here (no exists(), canonical(), ...).
// The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
	fs::path p;

	if (user_path.empty()) {
		*reason = "empty path";
		return false;
	}

	for (char c : user_path) {
		if (c == '\0') {
			*reason = "NUL character in path";
			return false;
		}
	}

	p = user_path;
	if (p.is_absolute() || p.has_root_path()) {
		*reason = "only workspace-relative paths are allowed";
		return false;
	}

	p = p.lexically_normal();
	if (*p.begin() == "..") {
		*reason = "path climbs above root";
		return false;
	} else if (p == ".")
		p = root();
	else
		p = root() / p;

	*out = p;
	return true;
}
