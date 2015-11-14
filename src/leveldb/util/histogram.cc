// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <math.h>
#include <stdio.h>
#include "port/port.h"
#include "util/histogram.h"

namespace leveldb {

const double histogram::kbucketlimit[knumbuckets] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20, 25, 30, 35, 40, 45,
  50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 200, 250, 300, 350, 400, 450,
  500, 600, 700, 800, 900, 1000, 1200, 1400, 1600, 1800, 2000, 2500, 3000,
  3500, 4000, 4500, 5000, 6000, 7000, 8000, 9000, 10000, 12000, 14000,
  16000, 18000, 20000, 25000, 30000, 35000, 40000, 45000, 50000, 60000,
  70000, 80000, 90000, 100000, 120000, 140000, 160000, 180000, 200000,
  250000, 300000, 350000, 400000, 450000, 500000, 600000, 700000, 800000,
  900000, 1000000, 1200000, 1400000, 1600000, 1800000, 2000000, 2500000,
  3000000, 3500000, 4000000, 4500000, 5000000, 6000000, 7000000, 8000000,
  9000000, 10000000, 12000000, 14000000, 16000000, 18000000, 20000000,
  25000000, 30000000, 35000000, 40000000, 45000000, 50000000, 60000000,
  70000000, 80000000, 90000000, 100000000, 120000000, 140000000, 160000000,
  180000000, 200000000, 250000000, 300000000, 350000000, 400000000,
  450000000, 500000000, 600000000, 700000000, 800000000, 900000000,
  1000000000, 1200000000, 1400000000, 1600000000, 1800000000, 2000000000,
  2500000000.0, 3000000000.0, 3500000000.0, 4000000000.0, 4500000000.0,
  5000000000.0, 6000000000.0, 7000000000.0, 8000000000.0, 9000000000.0,
  1e200,
};

void histogram::clear() {
  min_ = kbucketlimit[knumbuckets-1];
  max_ = 0;
  num_ = 0;
  sum_ = 0;
  sum_squares_ = 0;
  for (int i = 0; i < knumbuckets; i++) {
    buckets_[i] = 0;
  }
}

void histogram::add(double value) {
  // linear search is fast enough for our usage in db_bench
  int b = 0;
  while (b < knumbuckets - 1 && kbucketlimit[b] <= value) {
    b++;
  }
  buckets_[b] += 1.0;
  if (min_ > value) min_ = value;
  if (max_ < value) max_ = value;
  num_++;
  sum_ += value;
  sum_squares_ += (value * value);
}

void histogram::merge(const histogram& other) {
  if (other.min_ < min_) min_ = other.min_;
  if (other.max_ > max_) max_ = other.max_;
  num_ += other.num_;
  sum_ += other.sum_;
  sum_squares_ += other.sum_squares_;
  for (int b = 0; b < knumbuckets; b++) {
    buckets_[b] += other.buckets_[b];
  }
}

double histogram::median() const {
  return percentile(50.0);
}

double histogram::percentile(double p) const {
  double threshold = num_ * (p / 100.0);
  double sum = 0;
  for (int b = 0; b < knumbuckets; b++) {
    sum += buckets_[b];
    if (sum >= threshold) {
      // scale linearly within this bucket
      double left_point = (b == 0) ? 0 : kbucketlimit[b-1];
      double right_point = kbucketlimit[b];
      double left_sum = sum - buckets_[b];
      double right_sum = sum;
      double pos = (threshold - left_sum) / (right_sum - left_sum);
      double r = left_point + (right_point - left_point) * pos;
      if (r < min_) r = min_;
      if (r > max_) r = max_;
      return r;
    }
  }
  return max_;
}

double histogram::average() const {
  if (num_ == 0.0) return 0;
  return sum_ / num_;
}

double histogram::standarddeviation() const {
  if (num_ == 0.0) return 0;
  double variance = (sum_squares_ * num_ - sum_ * sum_) / (num_ * num_);
  return sqrt(variance);
}

std::string histogram::tostring() const {
  std::string r;
  char buf[200];
  snprintf(buf, sizeof(buf),
           "count: %.0f  average: %.4f  stddev: %.2f\n",
           num_, average(), standarddeviation());
  r.append(buf);
  snprintf(buf, sizeof(buf),
           "min: %.4f  median: %.4f  max: %.4f\n",
           (num_ == 0.0 ? 0.0 : min_), median(), max_);
  r.append(buf);
  r.append("------------------------------------------------------\n");
  const double mult = 100.0 / num_;
  double sum = 0;
  for (int b = 0; b < knumbuckets; b++) {
    if (buckets_[b] <= 0.0) continue;
    sum += buckets_[b];
    snprintf(buf, sizeof(buf),
             "[ %7.0f, %7.0f ) %7.0f %7.3f%% %7.3f%% ",
             ((b == 0) ? 0.0 : kbucketlimit[b-1]),      // left
             kbucketlimit[b],                           // right
             buckets_[b],                               // count
             mult * buckets_[b],                        // percentage
             mult * sum);                               // cumulative percentage
    r.append(buf);

    // add hash marks based on percentage; 20 marks for 100%.
    int marks = static_cast<int>(20*(buckets_[b] / num_) + 0.5);
    r.append(marks, '#');
    r.push_back('\n');
  }
  return r;
}

}  // namespace leveldb
