#include "reassembler.hh"

using namespace std;

/**
 * @brief Flushes in-order bytes from #unassembled_ into the ByteStream.
 *
 * Synchronizes #next_index_ with bytes_pushed(), then while a segment starts at
 * #next_index_, moves it out of the map and pushes in chunks bounded by
 * available_capacity(). If the stream is full, stores the unsent suffix back at
 * #next_index_ and stops. Closes the writer when #eof_index_ is set and all
 * bytes through EOF have been pushed.
 */
void Reassembler::push_contiguous() {
  next_index_ = output_.writer().bytes_pushed();

  while (true) {
    auto it = unassembled_.find(next_index_);
    if (it == unassembled_.end()) {
      break;
    }

    string chunk = move(it->second);
    unassembled_.erase(it);

    while (!chunk.empty() && output_.writer().available_capacity() > 0) {
      const size_t n = min(chunk.size(), output_.writer().available_capacity());
      output_.writer().push(chunk.substr(0, n));
      chunk.erase(0, n);
      next_index_ += n;
    }

    if (!chunk.empty()) {
      unassembled_.emplace(next_index_, move(chunk));
      break;
    }
  }

  if (eof_index_.has_value() && next_index_ >= *eof_index_) {
    output_.writer().close();
  }
}

/**
 * @brief Inserts a substring: trim, enforce capacity, merge overlaps, then
 * flush.
 *
 * @param first_index See Reassembler::insert().
 * @param data See Reassembler::insert().
 * @param is_last_substring See Reassembler::insert().
 */
void Reassembler::insert(uint64_t first_index, string data,
                         bool is_last_substring) {
  // Record exclusive end index [0, eof_index_) when the final segment is seen.
  if (is_last_substring) {
    eof_index_ = first_index + data.size();
  }

  next_index_ = output_.writer().bytes_pushed();

  // Drop bytes already pushed (duplicate or overlapping retransmissions).
  if (first_index < next_index_) {
    const uint64_t skip = next_index_ - first_index;
    if (skip >= data.size()) {
      push_contiguous();
      return;
    }
    data.erase(0, skip);
    first_index = next_index_;
  }

  // Capacity window (ASCII diagram in reassembler.hh):
  //   first_unacceptable = next_index_ + available_capacity()
  // Accept indices in [next_index_, first_unacceptable); discard the rest.
  const uint64_t first_unacceptable =
      next_index_ + output_.writer().available_capacity();
  if (first_index >= first_unacceptable) {
    push_contiguous();
    return;
  }
  if (first_index + data.size() > first_unacceptable) {
    data.resize(first_unacceptable - first_index);
  }

  // Merge with any overlapping stored segments (at most one disjoint entry per
  // index).
  for (auto it = unassembled_.begin(); it != unassembled_.end();) {
    const uint64_t seg_index = it->first;
    const string& seg = it->second;
    const uint64_t seg_end = seg_index + seg.size();

    if (seg_end <= first_index || seg_index >= first_index + data.size()) {
      ++it;
      continue;
    }

    // Union of intervals; later writes overwrite the same slot in merged.
    const uint64_t merged_index = min(first_index, seg_index);
    const uint64_t merged_end = max(first_index + data.size(), seg_end);
    string merged(merged_end - merged_index, '\0');

    for (uint64_t i = 0; i < data.size(); ++i) {
      merged[first_index - merged_index + i] = data[i];
    }
    for (uint64_t i = 0; i < seg.size(); ++i) {
      merged[seg_index - merged_index + i] = seg[i];
    }

    first_index = merged_index;
    data = move(merged);
    it = unassembled_.erase(it);
  }

  if (!data.empty()) {
    unassembled_.emplace(first_index, move(data));
  }

  // New bytes may have filled gaps; flush and maybe close.
  push_contiguous();
}

/**
 * @brief Sums the lengths of all segments in #unassembled_.
 * @return Number of bytes pending in the reassembler.
 */
uint64_t Reassembler::count_bytes_pending() const {
  uint64_t pending = 0;
  for (const auto& [index, chunk] : unassembled_) {
    (void)index;
    pending += chunk.size();
  }
  return pending;
}
