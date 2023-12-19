#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "zeus/queue.hpp"

/// Represents a custom object with an integer value.
/// Provides default, parameterized, copy, and move constructors, along with
/// copy and move assignment operators.
class CustomObject {
 public:
  CustomObject() noexcept : value(0) {}

  explicit CustomObject(int v) noexcept : value(v) {}

  CustomObject(const CustomObject& other) noexcept : value(other.value) {}

  CustomObject(CustomObject&& other) noexcept : value(other.value) {}

  CustomObject& operator=(const CustomObject& other) noexcept {
    if (this != &other) {
      value = other.value;
    }
    return *this;
  }

  CustomObject& operator=(CustomObject&& other) noexcept {
    if (this != &other) {
      value = other.value;
    }
    return *this;
  }

  ~CustomObject() noexcept = default;

 public:
  int value;
};

/// Test fixture lock-free queue tests, providing a queue instance for each
/// test.
class QueueTest : public ::testing::Test {
 protected:
  zeus::queue<int> testQueue{10};
  zeus::queue<CustomObject> objectQueue{10};
};

/// Verifies that a queue can be constructed and destructed without throwing
/// exceptions.
TEST_F(QueueTest, ConstructDestruct) {
  ASSERT_NO_THROW(zeus::queue<int> queue(5));
}

/// Tests that elements can be successfully emplaced into the queue,
/// increasing its size.
TEST_F(QueueTest, Emplace) {
  testQueue.emplace(1);
  ASSERT_EQ(testQueue.size(), 1);
}

/// Tests that elements can be emplaced up to the queue's capacity, and
/// rejects overfill.
TEST_F(QueueTest, TryEmplace) {
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(testQueue.try_emplace(i));
  }
  ASSERT_FALSE(testQueue.try_emplace(11));
}

/// Tests the queue's ability to handle push operations with move semantics
/// correctly.
TEST_F(QueueTest, PushMove) {
  int value = 3;
  testQueue.push(std::move(value));
  ASSERT_EQ(testQueue.size(), 1);
}

/// Tests the queue's push operation using copy semantics and checks for size
/// increment.
TEST_F(QueueTest, PushCopy) {
  int value = 4;
  testQueue.push(value);
  ASSERT_EQ(testQueue.size(), 1);
}

/// Tests the queue's capacity to handle push operations without exceeding its
/// size limit.
TEST_F(QueueTest, TryPush) {
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(testQueue.try_push(i));
  }
  ASSERT_FALSE(testQueue.try_push(12));
}

/// Tests the pop operation's ability to correctly remove and return the front
/// element.
TEST_F(QueueTest, Pop) {
  testQueue.emplace(6);
  ASSERT_EQ(testQueue.pop(), 6);
  ASSERT_TRUE(testQueue.empty());
}

/// Tests the try_pop functionality to conditionally pop elements, ensuring
/// correct behavior.
TEST_F(QueueTest, TryPop) {
  testQueue.emplace(7);
  auto result = testQueue.try_pop();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value(), 7);
  ASSERT_TRUE(testQueue.empty());
}

/// Tests that the queue's size function accurately reflects the number of
/// elements it contains.
TEST_F(QueueTest, Size) {
  ASSERT_EQ(testQueue.size(), 0);
  testQueue.emplace(8);
  ASSERT_EQ(testQueue.size(), 1);
}

/// Tests the empty function's ability to accurately report the queue's
/// emptiness status.
TEST_F(QueueTest, Empty) {
  ASSERT_TRUE(testQueue.empty());
  testQueue.emplace(9);
  ASSERT_FALSE(testQueue.empty());
}

/// Tests concurrent enqueueing and dequeueing, ensuring thread-safe operations.
TEST_F(QueueTest, ConcurrentAccess) {
  constexpr int NUM_THREADS = 5;
  constexpr int NUM_OPERATIONS = 5;
  std::vector<std::thread> producers, consumers;

  // Create threads for producing (enqueueing) elements
  for (int i = 0; i < NUM_THREADS; ++i) {
    producers.emplace_back([&] {
      for (int j = 0; j < NUM_OPERATIONS; ++j) {
        // Each producer thread enqueues a set number of elements
        testQueue.push(j);
      }
    });

    // Create threads for consuming (dequeuing) elements
    consumers.emplace_back([&] {
      for (int j = 0; j < NUM_OPERATIONS; ++j) {
        // Each consumer thread attempts to dequeue elements
        auto value = testQueue.try_pop();
        // If dequeueing fails, it keeps trying until it succeeds
        while (!value) {
          value = testQueue.try_pop();
        }
      }
    });
  }

  // Wait for all producer threads to finish
  for (auto& producer : producers) {
    producer.join();
  }

  // Wait for all consumer threads to finish
  for (auto& consumer : consumers) {
    consumer.join();
  }

  // Ensure that the queue is empty after all operations are complete
  ASSERT_TRUE(testQueue.empty());
}

/// Confirms the queue's capability to handle custom objects in concurrent
/// scenarios.
TEST_F(QueueTest, CustomObjectQueue) {
  std::vector<std::thread> producers, consumers;
  constexpr int NUM_THREADS = 5;
  constexpr int NUM_OPERATIONS = 5;

  // Create threads for producing (enqueueing) custom objects
  for (int i = 0; i < NUM_THREADS; ++i) {
    producers.emplace_back([&, i] {
      for (int j = 0; j < NUM_OPERATIONS; ++j) {
        // Each producer thread enqueues custom objects with unique values
        objectQueue.push(CustomObject(i * NUM_OPERATIONS + j));
      }
    });

    // Create threads for consuming (dequeuing) custom objects
    consumers.emplace_back([&, i] {
      for (int j = 0; j < NUM_OPERATIONS; ++j) {
        // Each consumer thread attempts to dequeue custom objects
        auto value = objectQueue.try_pop();
        // If dequeueing fails, it keeps trying until it succeeds
        while (!value) {
          value = objectQueue.try_pop();
        }
        // Verify that the dequeued object has the expected value
        ASSERT_EQ(value->value, i * NUM_OPERATIONS + j);
      }
    });
  }

  // Wait for all producer threads to finish
  for (auto& producer : producers) {
    producer.join();
  }

  // Wait for all consumer threads to finish
  for (auto& consumer : consumers) {
    consumer.join();
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
