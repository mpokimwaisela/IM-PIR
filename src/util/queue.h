// #include <algorithm>
// #include <condition_variable>
// #include <cstring>
// #include <iostream>
// #include <mutex>
// #include <queue>
// #include <thread>
// #include <vector>

// template <typename T>
// class ThreadSafeQueue {
// public:
//   // Push an item into the queue and wake one waiting thread.
//   void push(const T &item) {
//     {
//       std::lock_guard<std::mutex> lock(mtx);
//       q.push(item);
//       // std::cout << "Pushed item" << std::endl;
//     }
//     cv.notify_one();
//   }

//   // Pop an item if available. Returns false immediately if the queue is empty
//   // and the "done" flag is set.
//   bool pop(T &item) {
//     std::unique_lock<std::mutex> lock(mtx);
//     // Wait until there is an item in the queue or the done flag is set.
//     cv.wait(lock, [this] { return !q.empty() || done; });

//     // If the queue is empty and no more work will be produced, exit.
//     if (q.empty() && done) { 
//       return false;
//     }

//     // Otherwise, retrieve the item.
//     item = std::move(q.front());
//     q.pop();
//     // std::cout << "Popped item" << std::endl;
//     return true;
//   }

//   // Set the flag indicating that no more items will be pushed.
//   // Wake up all waiting threads so they can check the "done" flag.
//   void set_done() {
//     {
//       std::lock_guard<std::mutex> lock(mtx);
//       done = true;
//     }
//     cv.notify_all();
//   }

// private:
//   std::queue<T> q;
//   std::mutex mtx;
//   std::condition_variable cv;
//   bool done = false;
// };

// struct BatchData {
//   std::vector<uint8_t> query;
//   std::vector<std::vector<uint8_t>> dpu_input_vectors;
// };

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <optional>

template <typename T>
class ThreadSafeQueue {
public:
  // Push an item into the queue and wake one waiting thread.
  void push(const T &item) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      q.push(item);
    }
    cv.notify_one();
  }

  // Blocking pop. Returns false if done and queue is empty.
  bool pop(T &item) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !q.empty() || done; });
    if (q.empty() && done) return false;
    item = std::move(q.front());
    q.pop();
    return true;
  }

  // Non-blocking pop (for work stealing)
  bool try_pop(T &item) {
    std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
    if (!lock || q.empty()) return false;
    item = std::move(q.front());
    q.pop();
    return true;
  }

  // Mark this queue as done (no more pushes expected)
  void set_done() {
    {
      std::lock_guard<std::mutex> lock(mtx);
      done = true;
    }
    cv.notify_all();
  }

  // Query if queue is empty (non-blocking)
  bool empty() const {
    std::lock_guard<std::mutex> lock(mtx);
    return q.empty();
  }

private:
  std::queue<T> q;
  mutable std::mutex mtx;
  std::condition_variable cv;
  bool done = false;
};

struct BatchData {
  std::vector<uint8_t> query;
  std::vector<std::vector<uint8_t>> dpu_input_vectors;
};