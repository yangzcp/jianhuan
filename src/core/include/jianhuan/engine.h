#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jianhuan {

struct Candidate {
  std::string text;
  std::string comment;
};

struct Context {
  std::string preedit;
  int cursor_pos = 0;
  int sel_start = 0;
  int sel_end = 0;
  std::string commit_preview;
  int page_no = 0;
  int page_size = 0;
  bool is_last_page = false;
  int highlighted = 0;
  std::vector<Candidate> candidates;
  std::string select_keys;
};

class Engine {
 public:
  static Engine& instance();

  bool initialize(const std::string& shared_data_dir,
                  const std::string& user_data_dir);
  void shutdown();
  bool initialized() const;
  std::string user_data_dir() const;

  std::uint64_t create_session();
  void destroy_session(std::uint64_t session);

  bool process_key(std::uint64_t session, int keycode, int mask);
  bool commit_composition(std::uint64_t session);
  void clear_composition(std::uint64_t session);

  bool get_commit(std::uint64_t session, std::string& text);
  bool get_context(std::uint64_t session, Context& context);
  bool is_composing(std::uint64_t session);

  bool select_candidate(std::uint64_t session, std::size_t index);
  bool select_candidate_on_current_page(std::uint64_t session, std::size_t index);
  bool change_page(std::uint64_t session, bool backward);

  void set_option(std::uint64_t session, const std::string& option, bool value);
  bool get_option(std::uint64_t session, const std::string& option);
  bool select_schema(std::uint64_t session, const std::string& schema_id);
  std::string current_schema(std::uint64_t session);

 private:
  Engine() = default;
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace jianhuan