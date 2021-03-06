#ifndef ESPCXX_DATA_BUFFER_H_
#define ESPCXX_DATA_BUFFER_H_

#include <algorithm>
#include <atomic>
#include <mutex>

#include "esp_cxx/mutex.h"

namespace esp_cxx {

// Typesafe, locked, ringbuffer. Suitable for use in communicating between
// two tasks. Particularly useful for passing around std::unique_ptr<> as
// discarded elements will be returned back to the caller as temporary which
// can then trigger normal RAII clean-up.
//
// This has different semantics from the ESP-IDF FreeRTOS RingBuffer
// implemenation. In particular, it expects only one data type and
// inserting is actually an exchange operation.
template <typename T, size_t size>
class DataBuffer {
 public:
  uint32_t dropped_elements() const {
    std::lock_guard<Mutex> lock(mutex_);
    return dropped_elements_;
  }

  // Adds |obj| into the DataBuffer. If this overwrites 
  T Put(T&& obj) {
    std::lock_guard<Mutex> lock(mutex_);

    std::swap(data_[queue_head_], obj);
    queue_head_ = (queue_head_ + 1) % size;

    // If num_items_ == size, then this is an overwrite, not
    // an add.
    if (num_items_ < size) {
      num_items_++;
    } else {
      dropped_elements_++;
    }

    return std::move(obj);
  }

  std::optional<T> Get() {
    std::lock_guard<Mutex> lock(mutex_);

    if (num_items_ == 0) {
      return {};
    }

    size_t offset = queue_head_ - num_items_;
    // The head is larger than the tail. Queue is wrapped.
    if (offset > size) {
      offset = size - (num_items_ - queue_head_);
    }

    num_items_--;
    return std::move(data_[offset]);
  }

  size_t NumItems() {
    std::lock_guard<Mutex> lock(mutex_);
    return num_items_;
  }

 private:
  Mutex mutex_;

  // Number of elements dropped from this queue.
  uint32_t dropped_elements_{0};

  // Position to insert the next element.
  size_t queue_head_ = 0;
  
  // Current number of items in the queue.
  size_t num_items_ = 0;

  // Actual data inside the queue.
  std::array<T, size> data_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_DATA_BUFFER_H_

