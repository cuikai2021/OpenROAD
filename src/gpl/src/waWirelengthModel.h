#pragma once

namespace gpl {

// Weighted Average Wirelenght model of a net
class WAWireLengthModel
{
 public:
  WAWireLengthModel()
      : waExpMinSum_(0), waXExpMinSum_(0), waExpMaxSum_(0), waXExpMaxSum_(0)
  {
  }

  void clear()
  {
    waExpMinSum_ = 0;
    waXExpMinSum_ = 0;

    waExpMaxSum_ = 0;
    waXExpMaxSum_ = 0;
  }

  inline float waExpMinSum() const { return waExpMinSum_; }
  inline float waXExpMinSum() const { return waXExpMinSum_; }
  inline float waExpMaxSum() const { return waExpMaxSum_; }
  inline float waXExpMaxSum() const { return waXExpMaxSum_; }

  inline void addWaExpMinSum(float v) { waExpMinSum_ += v; }
  inline void addWaXExpMinSum(float v) { waXExpMinSum_ += v; }
  inline void addWaExpMaxSum(float v) { waExpMaxSum_ += v; }
  inline void addWaXExpMaxSum(float v) { waXExpMaxSum_ += v; }

 private:
  //
  // weighted average WL model store for better indexing
  // Please check the equation (4) in the ePlace-MS paper.
  //
  // WA: weighted Average
  // saving four variable will be helpful for
  // calculating the WA gradients/wirelengths.
  //
  // gamma: modeling accuracy.
  //
  // X forces.
  //
  // waExpMinSum_:  store sigma {exp(x_i/gamma)}
  // waXExpMinSum_: store sigma {x_i*exp(x_i/gamma)}
  // waExpMaxSum_ : store sigma {exp(-x_i/gamma)}
  // waXExpMaxSum_: store sigma {x_i*exp(-x_i/gamma)}
  //
  float waExpMinSum_;
  float waXExpMinSum_;

  float waExpMaxSum_;
  float waXExpMaxSum_;
};

}  // namespace gpl
