#pragma once

namespace lrl {

class Options
{
 public:
  // getter
  float uncertainty() { return uncertainty_; }
  float slackMargin() { return slack_margin_; }

  bool ignoreClk() { return ignore_clk_; }
  bool getDebugReport() { return debug_report_; }

  const char* spefFileName() { return spef_file; }

  // setter
  void setUncertainty(float uncertainty) { uncertainty_ = uncertainty; }
  void setSlackMargin(float slack_margin) { slack_margin_ = slack_margin; }

  void analyzeClk() { ignore_clk_ = false; }
  void debugReport() { debug_report_ = true; }

  void setSpefFile(const char* file_name) { spef_file = file_name; }

 private:
  float uncertainty_ = 0.1f;
  float slack_margin_ = 0.0f;

  bool ignore_clk_ = true;
  bool debug_report_ = false;

  const char* spef_file;
};

}  // namespace lrl