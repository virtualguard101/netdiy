#include "byte_stream.hh"

#include <cstddef>

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity), buffer_() {}

void Writer::push(string data) {
  /// @brief Push data to the stream, but only as much as available capacity
  /// allows.
  const size_t n = min(data.size(), static_cast<size_t>(available_capacity()));

  if (n == 0) {
    return;
  }
  buffer_.append(data.data(), n);
  bytes_pushed_ += n;
}

void Writer::close() { is_closed_ = true; }

bool Writer::is_closed() const { return is_closed_; }

uint64_t Writer::available_capacity() const {
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const { return bytes_pushed_; }

string_view Reader::peek() const { return buffer_; }

void Reader::pop(uint64_t len) {
  const size_t n = min(static_cast<size_t>(len), buffer_.size());
  buffer_.erase(0, n);
  bytes_popped_ += n;
}

bool Reader::is_finished() const { return is_closed_ && buffer_.empty(); }

uint64_t Reader::bytes_buffered() const { return buffer_.size(); }

uint64_t Reader::bytes_popped() const { return bytes_popped_; }
