#pragma once

#include <string>

namespace jianhuan {

struct Settings {
  std::string schema_id = "jianhuan_pinyin";
  bool ascii_mode = false;
  bool full_shape = false;
  bool ascii_punct = false;
  int page_size = 5;
};

bool load_settings(const std::string& user_data_dir, Settings& settings);
bool save_settings(const std::string& user_data_dir, const Settings& settings);

}  // namespace jianhuan