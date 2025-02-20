#pragma once

#include <iosfwd>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/count.hpp>

namespace precice {
namespace utils {
namespace statistics {

/**
 * Accunulates distance measures and provides statistics based on them.
 */
class DistanceAccumulator {
public:
  /// Accumulates value
  void operator()(double value)
  {
    _acc(value);
  }

  /// Returns the minimum of all accumulated values
  double min() const
  {
    return boost::accumulators::extract::min(_acc);
  }

  /// Returns the maximum of all accumulated values
  double max() const
  {
    return boost::accumulators::extract::max(_acc);
  }

  /// Returns the mean of all accumulated values
  double mean() const
  {
    return boost::accumulators::extract::mean(_acc);
  }

  /// Returns how many values have been accumulated
  std::size_t count() const
  {
      return boost::accumulators::extract::count(_acc);
  }

  /// Returns the sample variance based on all accumulated values
  double variance() const
  {
    return boost::accumulators::extract::variance(_acc);
  }

private:
  boost::accumulators::accumulator_set<double, boost::accumulators::stats<
      boost::accumulators::tag::min,
      boost::accumulators::tag::max,
      boost::accumulators::tag::mean,
      boost::accumulators::tag::lazy_variance
          >> _acc;
};

inline std::ostream &operator<<(std::ostream &out, const DistanceAccumulator &accumulator)
{
  out << "min:" << accumulator.min()
      << " max:" << accumulator.max()
      << " avg: " << accumulator.mean()
      << " var: " << accumulator.variance()
      << " cnt: " << accumulator.count();
  return out;
};

} // namespace statistics
} // namespace utils
} // namespace precice
