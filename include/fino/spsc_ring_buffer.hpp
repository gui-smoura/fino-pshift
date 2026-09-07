#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <new>

namespace fino {

/**
 * @brief Lock-free, wait-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 *
 * Adheres strictly to real-time audio thread constraints:
 * - Zero allocations or deallocations during push/pop operations.
 * - Cacheline alignment (64 bytes) to avoid false sharing between threads.
 * - Thread-safe between exactly one producer and one consumer.
 */
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t minimum_capacity = 65536)
        : m_capacity(next_power_of_two(minimum_capacity))
        , m_mask(m_capacity - 1)
        , m_buffer(m_capacity)
    {
        m_write_index.store(0, std::memory_order_relaxed);
        m_read_index.store(0, std::memory_order_relaxed);
    }

    ~SpscRingBuffer() = default;

    // Non-copyable, non-movable to guarantee address stability for real-time threads
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    [[nodiscard]] size_t capacity() const noexcept {
        return m_capacity;
    }

    [[nodiscard]] size_t available_write() const noexcept {
        const size_t write_idx = m_write_index.load(std::memory_order_relaxed);
        const size_t read_idx = m_read_index.load(std::memory_order_acquire);
        return m_capacity - (write_idx - read_idx);
    }

    [[nodiscard]] size_t available_read() const noexcept {
        const size_t write_idx = m_write_index.load(std::memory_order_acquire);
        const size_t read_idx = m_read_index.load(std::memory_order_relaxed);
        return write_idx - read_idx;
    }

    /**
     * @brief Pushes elements into the ring buffer. (Called by Producer)
     * @param data Pointer to input elements.
     * @param count Number of elements to push.
     * @return Actual number of elements pushed (may be less than count if buffer fills).
     */
    size_t push(const T* data, size_t count) noexcept {
        if (data == nullptr || count == 0) {
            return 0;
        }

        const size_t write_idx = m_write_index.load(std::memory_order_relaxed);
        const size_t read_idx = m_read_index.load(std::memory_order_acquire);
        const size_t free_space = m_capacity - (write_idx - read_idx);

        const size_t to_write = std::min(count, free_space);
        if (to_write == 0) {
            return 0;
        }

        const size_t start_pos = write_idx & m_mask;
        const size_t first_chunk = std::min(to_write, m_capacity - start_pos);
        const size_t second_chunk = to_write - first_chunk;

        std::copy_n(data, first_chunk, m_buffer.data() + start_pos);
        if (second_chunk > 0) {
            std::copy_n(data + first_chunk, second_chunk, m_buffer.data());
        }

        m_write_index.store(write_idx + to_write, std::memory_order_release);
        return to_write;
    }

    /**
     * @brief Pops elements from the ring buffer. (Called by Consumer)
     * @param destination Pointer to destination buffer.
     * @param count Number of elements to pop.
     * @return Actual number of elements popped (may be less than count if buffer starves).
     */
    size_t pop(T* destination, size_t count) noexcept {
        if (destination == nullptr || count == 0) {
            return 0;
        }

        const size_t write_idx = m_write_index.load(std::memory_order_acquire);
        const size_t read_idx = m_read_index.load(std::memory_order_relaxed);
        const size_t readable = write_idx - read_idx;

        const size_t to_read = std::min(count, readable);
        if (to_read == 0) {
            return 0;
        }

        const size_t start_pos = read_idx & m_mask;
        const size_t first_chunk = std::min(to_read, m_capacity - start_pos);
        const size_t second_chunk = to_read - first_chunk;

        std::copy_n(m_buffer.data() + start_pos, first_chunk, destination);
        if (second_chunk > 0) {
            std::copy_n(m_buffer.data(), second_chunk, destination + first_chunk);
        }

        m_read_index.store(read_idx + to_read, std::memory_order_release);
        return to_read;
    }

    /**
     * @brief Reset read and write pointers. Call only when threads are synchronized / idle.
     */
    void clear() noexcept {
        m_write_index.store(0, std::memory_order_release);
        m_read_index.store(0, std::memory_order_release);
    }

private:
    static constexpr size_t next_power_of_two(size_t value) noexcept {
        if (value <= 1) return 2;
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;
        return value + 1;
    }

    const size_t m_capacity;
    const size_t m_mask;
    std::vector<T> m_buffer;

    // Cacheline separation to eliminate false sharing between producer and consumer cores
    alignas(64) std::atomic<size_t> m_write_index{0};
    alignas(64) std::atomic<size_t> m_read_index{0};
};

} // namespace fino
