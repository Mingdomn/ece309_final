#pragma once

/*
 * Files that include this one need this.
 * Since I can't modify them, just include it here.
 */
#include <string>

#include <core/message.h>

class Conversation {
private:
	Message *data_ = nullptr;
	std::size_t size_ = 0;
	std::size_t capacity_ = 0;
public:
	Conversation();

	~Conversation();

	/* copy */
	Conversation(const Conversation &other) noexcept;
	Conversation &operator=(const Conversation &other) noexcept;

	/* move */
	Conversation(Conversation &&other) noexcept;
	Conversation &operator=(Conversation &&other) noexcept;

	void append(Message m);

	std::size_t size() const noexcept;
	/* For the tests. */
	std::size_t capacity() const noexcept;

	const Message &at(std::size_t i) const;

	const Message *begin() const noexcept;
	const Message *end() const noexcept;
};
