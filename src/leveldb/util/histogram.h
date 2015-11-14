// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_util_histogram_h_
#define storage_leveldb_util_histogram_h_

#include <string>

namespace leveldb {

class histogram {
 public:
  histogram() { }
  ~histogram() { }

  void clear();
  void add(double value);
  void merge(const histogram& other);

  std::string tostring() const;

 private:
  double min_;
  double max_;
  double num_;
  double sum_;
  double sum_squares_;

  enum { knumbuckets = 154 };
  static const double kbucketlimit[knumbuckets];
  double buckets_[knumbuckets];

  double median() const;
  double percentile(double p) const;
  double average() const;
  double standarddeviation() const;
};

}  // namespace leveldb

#endif  // storage_leveldb_util_histogram_h_
