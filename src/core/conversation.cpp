#include <string>
#include <cstddef>

#include <core/conversation.h>

Conversation::Conversation() : size_(0), capacity_(1) {
	data_ = new Message[capacity_];
}

Conversation::~Conversation() {
	delete[] data_;
}

Conversation::Conversation(const Conversation &other) noexcept : size_(other.size_), capacity_(other.size_) {
	/*
	 * There's no need to allocate other's entire capacity.
	 * The array will grow if we need more.
	 */
	data_ = new Message[capacity_];

	for (std::size_t i = 0; i < size_; i++)
		data_[i] = other.data_[i];
}

Conversation::Conversation(Conversation &&other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
	other.size_ = 0;
	other.capacity_ = 1;
	other.data_ = new Message[other.capacity_];
}

Conversation &Conversation::operator=(const Conversation &other) noexcept {
	if (this == &other)
		return *this;

	/* Use placement new to call the copy constructor. */
	this->~Conversation();
	new (this) Conversation(other);
	return *this;
}

Conversation &Conversation::operator=(Conversation &&other) noexcept {
	if (this == &other)
		return *this;

	/* Use placement new to call the move constructor. */
	this->~Conversation();
	new (this) Conversation(std::move(other));
	return *this;
}

void Conversation::append(Message m) {
	/* Grow array if needed. */
	if (size_ == capacity_) {
		Message *tmp = data_;

		capacity_ = std::ceil(capacity_ * 1.5);
		data_ = new Message[capacity_];

		for (std::size_t i = 0; i < size_; i++)
			data_[i] = tmp[i];

		delete[] tmp;
	}

	data_[size_++] = m;
}

std::size_t Conversation::size() const noexcept {
	return size_;
}

std::size_t Conversation::capacity() const noexcept {
	return capacity_;
}

const Message &Conversation::at(std::size_t i) const {
	return data_[i];
}

const Message *Conversation::begin() const noexcept {
	return &data_[0];
}

const Message *Conversation::end() const noexcept {
	return &data_[size_];
}
