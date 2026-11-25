#include "jianhuan/settings.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace jianhuan {

namespace {

bool parse_bool(const std::string& value) {
  return value == "true" || value == "1";
}

}  // namespace

bool load_settings(const std::string& user_data_dir, Settings& settings) {
  settings = Settings{};
  const auto path = std::filesystem::path(user_data_dir) / "jianhuan.ini";
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(file, line)) {
    const auto separator = line.find('=');
    if (separator == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, separator);
    const std::string value = line.substr(separator + 1);
    if (key == "schema_id") {
      settings.schema_id = value;
    } else if (key == "ascii_mode") {
      settings.ascii_mode = parse_bool(value);
    } else if (key == "full_shape") {
      settings.full_shape = parse_bool(value);
    } else if (key == "ascii_punct") {
      settings.ascii_punct = parse_bool(value);
    } else if (key == "page_size") {
      settings.page_size = std::stoi(value);
    }
  }
  return true;
}

bool save_settings(const std::string& user_data_dir, const Settings& settings) {
  std::error_code ec;
  std::filesystem::create_directories(user_data_dir, ec);
  const auto path = std::filesystem::path(user_data_dir) / "jianhuan.ini";
  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open()) {
    return false;
  }
  file << "schema_id=" << settings.schema_id << "\n";
  file << "ascii_mode=" << (settings.ascii_mode ? "true" : "false") << "\n";
  file << "full_shape=" << (settings.full_shape ? "true" : "false") << "\n";
  file << "ascii_punct=" << (settings.ascii_punct ? "true" : "false") << "\n";
  file << "page_size=" << settings.page_size << "\n";
  return true;
}

}  // namespace jianhuan