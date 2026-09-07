#include <catch2/catch_test_macros.hpp>
#include "fino/spsc_ring_buffer.hpp"
#include <thread>
#include <vector>
#include <numeric>

TEST_CASE("SpscRingBuffer basic operations", "[ring_buffer]") {
    fino::SpscRingBuffer<float> buffer(1024);

    REQUIRE(buffer.capacity() == 1024);
    REQUIRE(buffer.available_read() == 0);
    REQUIRE(buffer.available_write() == 1024);

    std::vector<float> input(256, 1.234f);
    size_t written = buffer.push(input.data(), input.size());
    REQUIRE(written == 256);
    REQUIRE(buffer.available_read() == 256);
    REQUIRE(buffer.available_write() == 1024 - 256);

    std::vector<float> output(256, 0.0f);
    size_t read = buffer.pop(output.data(), output.size());
    REQUIRE(read == 256);
    REQUIRE(buffer.available_read() == 0);
    REQUIRE(output == input);
}

TEST_CASE("SpscRingBuffer boundary and wrap-around", "[ring_buffer]") {
    fino::SpscRingBuffer<int> buffer(64); // Power of two: 64

    // Push 48 elements, pop 48 elements (read pointer advances)
    std::vector<int> chunk1(48);
    std::iota(chunk1.begin(), chunk1.end(), 0);
    REQUIRE(buffer.push(chunk1.data(), 48) == 48);

    std::vector<int> out1(48);
    REQUIRE(buffer.pop(out1.data(), 48) == 48);
    REQUIRE(out1 == chunk1);

    // Push 40 elements (causes wrap-around across index 64)
    std::vector<int> chunk2(40);
    std::iota(chunk2.begin(), chunk2.end(), 100);
    REQUIRE(buffer.push(chunk2.data(), 40) == 40);

    std::vector<int> out2(40);
    REQUIRE(buffer.pop(out2.data(), 40) == 40);
    REQUIRE(out2 == chunk2);
}

TEST_CASE("SpscRingBuffer underflow and overflow limits", "[ring_buffer]") {
    fino::SpscRingBuffer<float> buffer(16);

    // Pop from empty
    float dummy = 0.0f;
    REQUIRE(buffer.pop(&dummy, 1) == 0);

    // Fill completely
    std::vector<float> data(16, 42.0f);
    REQUIRE(buffer.push(data.data(), 16) == 16);

    // Overflow attempt: should reject excess elements
    float excess = 99.0f;
    REQUIRE(buffer.push(&excess, 1) == 0);
    REQUIRE(buffer.available_write() == 0);
    REQUIRE(buffer.available_read() == 16);
}

TEST_CASE("SpscRingBuffer multi-threaded concurrent stress", "[ring_buffer][concurrency]") {
    fino::SpscRingBuffer<uint32_t> buffer(2048);
    const uint32_t total_items = 200000;

    std::vector<uint32_t> consumed;
    consumed.reserve(total_items);

    std::thread producer([&]() {
        uint32_t val = 0;
        while (val < total_items) {
            uint32_t batch[128];
            size_t batch_size = std::min<size_t>(128, total_items - val);
            for (size_t i = 0; i < batch_size; ++i) {
                batch[i] = val + static_cast<uint32_t>(i);
            }

            size_t written = 0;
            while (written < batch_size) {
                written += buffer.push(batch + written, batch_size - written);
                if (written < batch_size) {
                    std::this_thread::yield();
                }
            }
            val += static_cast<uint32_t>(batch_size);
        }
    });

    std::thread consumer([&]() {
        uint32_t count = 0;
        uint32_t batch[128];
        while (count < total_items) {
            size_t popped = buffer.pop(batch, 128);
            for (size_t i = 0; i < popped; ++i) {
                consumed.push_back(batch[i]);
            }
            count += static_cast<uint32_t>(popped);
            if (popped == 0) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(consumed.size() == total_items);
    for (uint32_t i = 0; i < total_items; ++i) {
        REQUIRE(consumed[i] == i);
    }
}
