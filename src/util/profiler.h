#ifndef PROFILER_H
#define PROFILER_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

class Profiler {
public:
  void start(const std::string &eventName) {
    startTimes[eventName] = std::chrono::high_resolution_clock::now();
  }

  void accumulate(const std::string &eventName) {
    auto now = std::chrono::high_resolution_clock::now();
    auto it = startTimes.find(eventName);
    if (it == startTimes.end()) {
      std::cerr << "Warning: accumulate() called for '" << eventName
                << "' without matching start()" << std::endl;
      return;
    }
    auto elapsed = now - it->second;
    events[eventName] += elapsed;
    counts[eventName]++;
    eventTimes[eventName].push_back(elapsed);
    startTimes[eventName] = now;
  }

  double getTotalTime(const std::string &eventName) const {
    auto it = events.find(eventName);
    return (it != events.end()) ? it->second.count() * 1000.0 : 0.0;
  }

  double getAverageTime(const std::string &eventName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto totalIt = events.find(eventName);
    auto countIt = counts.find(eventName);
    if (totalIt != events.end() && countIt != counts.end() && countIt->second > 0) {
      return (totalIt->second.count() * 1000.0) / countIt->second;
    }
    return 0.0;
  }

  double getMedianTime(const std::string &eventName) const {
    auto it = eventTimes.find(eventName);
    if (it != eventTimes.end() && !it->second.empty()) {
      auto times = it->second; // copy durations
      std::sort(times.begin(), times.end());
      size_t n = times.size();
      if (n % 2 == 1) {
        return times[n/2].count() * 1000.0;
      } else {
        return ((times[n/2 - 1].count() + times[n/2].count()) / 2.0) * 1000.0;
      }
    }
    return 0.0;
  }

  double getStdDevTime(const std::string &eventName) const {
    auto it = eventTimes.find(eventName);
    if (it == eventTimes.end() || it->second.empty()) return 0.0;
    // convert to milliseconds
    std::vector<double> ms;
    ms.reserve(it->second.size());
    for (const auto &d : it->second) {
      ms.push_back(d.count() * 1000.0);
    }
    double mean = std::accumulate(ms.begin(), ms.end(), 0.0) / ms.size();
    double var = 0.0;
    for (double x : ms) {
      var += (x - mean) * (x - mean);
    }
    var /= ms.size(); // population variance
    return std::sqrt(var);
  }

  // Print each event's average or median, with optional standard deviation
  void printAllTimes(bool useMedian = false, bool showStdDev = false) const {
    for (const auto &entry : eventTimes) {
      const std::string &eventName = entry.first;
      double center = useMedian ? getMedianTime(eventName) : getAverageTime(eventName);
      std::cout << eventName << " : " << center << " ms";
      if (showStdDev) {
        double sd = getStdDevTime(eventName);
        std::cout << " \u00B1 " << sd << " ms";
      }
      std::cout << std::endl;
    }
  }

  void reset() {
    startTimes.clear();
    events.clear();
    counts.clear();
    eventTimes.clear();
  }

  void remove(const std::string &eventName) {
    startTimes.erase(eventName);
    events.erase(eventName);
    counts.erase(eventName);
    eventTimes.erase(eventName);
  }

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> startTimes;
  std::unordered_map<std::string, std::chrono::duration<double>> events;
  std::unordered_map<std::string, size_t> counts;
  std::unordered_map<std::string, std::vector<std::chrono::duration<double>>> eventTimes;
};

#endif // PROFILER_H
