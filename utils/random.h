#include <string>
#include <cmath>
#include <stdint.h>

using std::string;

class Random {
public:
  Random(unsigned long seed, int start, int end) : seed(0) { 
    set_seed0(seed); 
    items = end - start + 1;
  }

  inline void init_zipfian(double factor = 0.90) {
    zipfconstant = factor;

    double theta = zipfconstant;
    alpha = 1.0 / (1.0 - theta);
    zeta_2 = zeta(2, theta);
    zeta_n = zeta(items, theta);
    eta = (1 - pow(2.0 / items, 1 - theta)) / (1 - zeta_2 / zeta_n);

    pow_const = pow(0.5, zipfconstant);
  }

  inline int next_uniform() {
    return (int)(next_uniform_long() % items + start);
  }

  // Should call `init_zipfian` before use this function!
  inline int next_zipfian() {
    double u = (next_uniform_long() % items) / (double)items;
    double uz = u * zeta_n;
    if (uz < 1.0) {
      return start;
    } else if (uz < 1.0 + pow_const) {
      return start + 1;
    } else {
      return start + int(items * pow(eta * u - eta + 1, alpha));
    }
  }

  inline double next_uniform_real() {
    return (((unsigned long)next(26) << 27) + next(27)) / (double)(1L << 53);
  }

private:
  inline unsigned long next(unsigned int bits) {
    seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1);
    return (unsigned long)(seed >> (48 - bits));
  }

  inline void set_seed0(unsigned long seed) {
    this->seed = (seed ^ 0x5DEECE66DL) & ((1L << 48) - 1);
  }

  inline double zeta(int upper, double theta, int low = 0) {
    double sum = 0.0;
    for (int i = low; i < upper; i++) {
      sum += 1.0 / pow((double)(i + 1), theta);
    }
    return sum;
  }

  inline unsigned long next_uniform_long() {
    return ((unsigned long)next(32) << 32) + next(32);
  }

  // Common member variables.
  unsigned long seed = 0;
  int start = 0;
  int end = 0;
  int items = 0;

  // Member variables of zipfian generator.
  double zipfconstant = 0.0;
  double alpha = 0.0;
  double zeta_2 = 0.0;
  double zeta_n = 0.0;
  double eta = 0.0;
  double pow_const = 0.0;
};
