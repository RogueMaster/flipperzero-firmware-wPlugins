#pragma once

#include <stddef.h>
#include <stdint.h>

namespace core {

// Storage held inside the queue object (static RAM when the queue is a global).
template <typename T, size_t Capacity>
struct InlineStorage {
    T items[Capacity];
    T* ptr() {
        return items;
    }
    const T* ptr() const {
        return items;
    }
};

// Storage allocated once from the heap when the queue is constructed. Use it for
// large queues: on the classic ESP32, static data must fit a single DRAM region,
// while the heap can also use the remaining DRAM.
template <typename T, size_t Capacity>
struct HeapStorage {
    T* items;
    HeapStorage()
        : items(new T[Capacity]) {
    }
    ~HeapStorage() {
        delete[] items;
    }
    HeapStorage(const HeapStorage&) = delete;
    HeapStorage& operator=(const HeapStorage&) = delete;
    T* ptr() {
        return items;
    }
    const T* ptr() const {
        return items;
    }
};

template <typename T, size_t Capacity, template <typename, size_t> class Storage = InlineStorage>
class RingQueue {
public:
    RingQueue()
        : head_(0)
        , tail_(0)
        , count_(0) {
    }

    bool push(const T& item) {
        if(count_ >= Capacity) {
            return false;
        }
        storage_.ptr()[head_] = item;
        head_ = (head_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool pop(T& out) {
        if(count_ == 0) {
            return false;
        }
        out = storage_.ptr()[tail_];
        tail_ = (tail_ + 1) % Capacity;
        --count_;
        return true;
    }

    bool pushDropOldest(const T& item, bool* dropped_oldest = nullptr) {
        bool dropped = false;
        if(count_ >= Capacity) {
            tail_ = (tail_ + 1) % Capacity;
            --count_;
            dropped = true;
        }
        (void)push(item);
        if(dropped_oldest != nullptr) {
            *dropped_oldest = dropped;
        }
        return true;
    }

    // Oldest item without removing it, or nullptr when empty.
    const T* front() const {
        return count_ == 0 ? nullptr : &storage_.ptr()[tail_];
    }

    bool empty() const {
        return count_ == 0;
    }
    bool full() const {
        return count_ >= Capacity;
    }
    size_t size() const {
        return count_;
    }
    size_t capacity() const {
        return Capacity;
    }
    size_t freeSlots() const {
        return Capacity - count_;
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

private:
    Storage<T, Capacity> storage_;
    size_t head_;
    size_t tail_;
    size_t count_;
};

} // namespace core
