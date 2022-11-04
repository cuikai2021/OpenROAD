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
  bool isWireBudModeRC() { return wire_bud_mode_rc_; }

  const char* spefFileName() { return spef_file_; }

  // setter
  void setUncertainty(float uncertainty) { uncertainty_ = uncertainty; }
  void setSlackMargin(float slack_margin) { slack_margin_ = slack_margin; }

  void analyzeClk() { ignore_clk_ = false; }
  void debugReport() { debug_report_ = true; }
  void setWireBudModeRC() { wire_bud_mode_rc_ = true; }
  void resetWireBudMode() { wire_bud_mode_rc_ = false; }

  void setSpefFile(const char* file_name) { spef_file_ = file_name; }

  // checkers
  bool hasSpefFile() { return spef_file_; }

 private:
  float uncertainty_ = 0.05f;
  float slack_margin_ = 0.0f;

  bool ignore_clk_ = true;
  bool debug_report_ = false;
  bool wire_bud_mode_rc_ = false;

  const char* spef_file_ = nullptr;
};

}  // namespace lrl