#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "byte_stream.hh"

/**
 * @brief Reassembles indexed substrings (possibly out-of-order or overlapping)
 * into one contiguous byte stream written to an internal ByteStream.
 *
 * Each byte in the final stream has a unique index starting at zero. For a full
 * stream "abcdef", @c insert(0, "ab", false) may push immediately, @c insert(4,
 * "ef", false) may be stored until gaps are filled, and @c insert(2, "cd",
 * false) can then flush "cdef" in order.
 *
 * Capacity rule (Checkpoint 1 handout): green (ByteStream buffer) + red
 * (#unassembled_) must not exceed capacity. Acceptable index window for new
 * bytes: [next_index_, first_unacceptable) with
 * first_unacceptable = next_index + writer.available_capacity().
 *
 * Example (capacity = 4, reader has not popped). Already pushed indices 0–1;
 * "b" stored at index 3; index 2 still unknown:
 *
 * @code
 *   stream index:     0     1     2     3     4     5     6
 *                     |     |     |     |     |     |     |
 *   committed:        [x]   [x]                 (index < next_index_, on
 * stream)
 *                           ^
 *                     next_index_ (= 2)
 *
 *   ByteStream buffer (green, bytes_buffered = 2):
 *                     [g0]  [g1]              pushed, not yet popped
 *
 *   unassembled_ (red, count_bytes_pending = 1):
 *                                   [r]         insert(3, "b") waiting for
 * index 2
 *
 *   memory budget:    |<------ green (2) + red (1) <= capacity (4) ------>|
 *   free push slots:  |<-- available_capacity() = 2 -->|
 *                     |                    first_unacceptable (= 4) ->|
 *
 *   insert(2, "c")     OK  (index 2 < 4)  -> merge, push "bc", ...
 *   insert(5, "x")     DROP (index 5 >= 4) even if gaps remain
 * @endcode
 */
class Reassembler {
 public:
  /**
   * @brief Constructs a reassembler that takes ownership of the given
   * ByteStream.
   * @param output ByteStream to write reassembled data into (moved-from).
   */
  explicit Reassembler(ByteStream&& output)
      : output_(std::move(output)),
        next_index_{},
        eof_index_{},
        unassembled_{} {}

  /**
   * @brief Inserts a new substring to be reassembled into the ByteStream.
   *
   * @param first_index Index of the first byte of this substring in the overall
   * stream.
   * @param data        Substring bytes (may be empty).
   * @param is_last_substring If true, this substring ends the stream (no bytes
   * beyond it).
   *
   * Pushes bytes to output_.writer() as soon as they are the next in-order
   * bytes. Bytes that fit within capacity but have leading gaps are stored in
   * #unassembled_. Bytes beyond the capacity window are discarded. Closes the
   * stream after the last byte is written when the end of the stream is known.
   */
  void insert(uint64_t first_index, std::string data, bool is_last_substring);

  /**
   * @brief Returns the number of bytes currently stored inside the reassembler.
   * @return Total size of all strings in #unassembled_ (for tests only).
   */
  uint64_t count_bytes_pending() const;

  /** @brief Access the output stream reader. */
  Reader& reader() { return output_.reader(); }

  /** @brief Access the output stream reader (const). */
  const Reader& reader() const { return output_.reader(); }

  /** @brief Access the output stream writer (const; external code cannot push).
   */
  const Writer& writer() const { return output_.writer(); }

 private:
  /**
   * @brief Pushes contiguous bytes starting at #next_index_ into the
   * ByteStream.
   *
   * Repeatedly takes the chunk at #next_index_ from #unassembled_, pushes as
   * much as available_capacity() allows, and re-stores any remainder. Closes
   * the writer when #eof_index_ is known and all bytes through EOF have been
   * pushed.
   */
  void push_contiguous();

  /** @brief Destination stream; the application reads via reader(). */
  ByteStream output_;

  /**
   * @brief Index of the next byte expected to be pushed.
   *
   * Invariant: equals output_.writer().bytes_pushed() after each
   * push_contiguous().
   */
  uint64_t next_index_{};

  /**
   * @brief Stream end position (one past the last byte), if known.
   *
   * Set when is_last_substring is true on an insert. std::nullopt until then.
   */
  std::optional<uint64_t> eof_index_{};

  /**
   * @brief Out-of-order segments waiting for earlier bytes.
   *
   * Key: starting index of a contiguous run. Value: run contents.
   * Invariant: no two entries overlap (overlapping inserts are merged first).
   */
  std::map<uint64_t, std::string> unassembled_{};
};
