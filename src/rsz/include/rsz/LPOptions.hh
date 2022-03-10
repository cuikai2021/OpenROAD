#pragma once

namespace rsz {

class LPOptions
{
 public:
  // getter
  float uncertainty() { return uncertainty_; }
  float slackMargin() { return slack_margin_; }

  // setter
  void setUncertainty(float uncertainty) { uncertainty_ = uncertainty; }
  void setSlackMargin(float slack_margin) { slack_margin_ = slack_margin; }

 private:
  float uncertainty_ = 0.1f;
  float slack_margin_ = 0.0f;
};

}  // namespace rsz