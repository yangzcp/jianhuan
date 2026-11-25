#include "jianhuan/engine.h"
#include "jianhuan/key_policy.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool has_candidate(const jianhuan::Context& context, const std::string& text) {
  for (const auto& candidate : context.candidates) {
    if (candidate.text == text) {
      return true;
    }
  }
  return false;
}

void type(jianhuan::Engine& engine, std::uint64_t session, const std::string& keys) {
  for (char c : keys) {
    engine.process_key(session, static_cast<unsigned char>(c), 0);
  }
}

}  // namespace

int main(int argc, char** argv) {
  using jianhuan::ShouldEatKeyPolicy;
  const bool eat = true;
  const bool pass = false;
  assert(ShouldEatKeyPolicy(false, false, false, 'A', true, false, false, 'a') == pass);
  assert(ShouldEatKeyPolicy(false, false, false, 'V', true, false, false, 'v') == pass);
  assert(ShouldEatKeyPolicy(false, false, false, 'C', true, false, false, 'c') == pass);
  assert(ShouldEatKeyPolicy(false, true, false, 'A', true, false, false, 'a') == pass);
  assert(ShouldEatKeyPolicy(false, false, false, 'A', false, false, false, 'a') == eat);
  assert(ShouldEatKeyPolicy(true, false, false, 'A', false, false, false, 'a') == pass);
  assert(ShouldEatKeyPolicy(false, false, false, VK_SPACE, false, false, false, ' ') == pass);
  assert(ShouldEatKeyPolicy(false, true, false, VK_SPACE, false, false, false, ' ') == eat);
  assert(ShouldEatKeyPolicy(false, false, false, VK_BACK, false, false, false, 0xff08) == pass);
  assert(ShouldEatKeyPolicy(false, true, false, VK_BACK, false, false, false, 0xff08) == eat);
  assert(ShouldEatKeyPolicy(false, false, false, VK_LEFT, false, false, false, 0xff51) == pass);
  assert(ShouldEatKeyPolicy(false, true, false, VK_LEFT, false, false, false, 0xff51) == eat);
  assert(ShouldEatKeyPolicy(false, false, true, VK_OEM_COMMA, false, false, false, ',') == pass);
  assert(ShouldEatKeyPolicy(false, false, false, VK_OEM_COMMA, false, false, false, ',') == eat);
  assert(ShouldEatKeyPolicy(false, true, false, VK_OEM_PLUS, false, false, false, '=') == eat);
  assert(ShouldEatKeyPolicy(false, false, false, VK_OEM_PLUS, false, false, false, '=') == pass);
  assert(ShouldEatKeyPolicy(false, false, false, '5', false, false, false, '5') == pass);
  assert(ShouldEatKeyPolicy(false, true, false, '5', false, false, false, '5') == eat);
  std::cerr << "key policy ok\n";

  if (argc < 2) {
    std::cerr << "usage: jianhuan_core_test <shared_data_dir>\n";
    return 1;
  }

  const std::string user_dir =
      (std::filesystem::temp_directory_path() / "jianhuan_core_test_user").string();
  std::error_code ec;
  std::filesystem::remove_all(user_dir, ec);

  auto& engine = jianhuan::Engine::instance();
  if (!engine.initialize(argv[1], user_dir)) {
    std::cerr << "initialize failed\n";
    return 1;
  }

  const std::uint64_t session = engine.create_session();
  if (session == 0) {
    std::cerr << "create_session failed\n";
    return 1;
  }

  if (!engine.select_schema(session, "jianhuan_pinyin")) {
    std::cerr << "select jianhuan_pinyin failed\n";
    return 1;
  }
  type(engine, session, "ni");
  jianhuan::Context context;
  if (!engine.get_context(session, context) || !has_candidate(context, "你")) {
    std::cerr << "full pinyin ni did not produce 你\n";
    return 1;
  }
  engine.clear_composition(session);

  type(engine, session, "ni");
  engine.process_key(session, 0xff08, 0);
  if (!engine.get_context(session, context) || context.preedit != "n") {
    std::cerr << "Backspace did not edit composition\n";
    return 1;
  }
  engine.clear_composition(session);
  if (engine.process_key(session, 0xff08, 0)) {
    std::cerr << "Backspace should pass through without composition\n";
    return 1;
  }

  engine.set_option(session, "ascii_punct", false);
  engine.process_key(session, ',', 0);
  std::string commit;
  if (!engine.get_commit(session, commit) || commit != "，") {
    std::cerr << "Chinese punctuation failed\n";
    return 1;
  }
  engine.set_option(session, "ascii_punct", true);
  if (engine.process_key(session, ',', 0)) {
    std::cerr << "ASCII punctuation should pass through\n";
    return 1;
  }
  engine.set_option(session, "ascii_punct", false);

  engine.clear_composition(session);
  auto select_char = [&](const std::string& ch) {
    for (int page = 0; page < 10; ++page) {
      jianhuan::Context c;
      if (!engine.get_context(session, c)) return false;
      for (std::size_t i = 0; i < c.candidates.size(); ++i) {
        if (c.candidates[i].text == ch) {
          return engine.select_candidate_on_current_page(session, i);
        }
      }
      if (c.is_last_page) return false;
      engine.change_page(session, false);
    }
    return false;
  };
  type(engine, session, "jianhuan");
  select_char("简");
  select_char("焕");
  engine.process_key(session, 0x20, 0);
  std::string learned;
  engine.get_commit(session, learned);
  std::cerr << "learned commit: " << learned << "\n";
  engine.clear_composition(session);
  type(engine, session, "jianhuan");
  if (!engine.get_context(session, context) || !has_candidate(context, "简焕")) {
    std::cerr << "user dictionary did not learn new phrase; candidates:";
    for (const auto& candidate : context.candidates) std::cerr << " " << candidate.text;
    std::cerr << "\n";
    engine.destroy_session(session);
    engine.shutdown();
    return 1;
  }
  engine.clear_composition(session);

  type(engine, session, "haha");
  if (!engine.get_context(session, context) ||
      !has_candidate(context, std::string("\xF0\x9F\x98\x84"))) {
    std::cerr << "emoji candidate missing for haha\n";
    engine.destroy_session(session);
    engine.shutdown();
    return 1;
  }
  engine.clear_composition(session);

  if (!engine.select_schema(session, "jianhuan_double_pinyin")) {
    std::cerr << "select jianhuan_double_pinyin failed\n";
    return 1;
  }
  type(engine, session, "vs");
  if (!engine.get_context(session, context) || !has_candidate(context, "中")) {
    std::cerr << "double pinyin vs did not produce 中\n";
    return 1;
  }
  engine.clear_composition(session);

  engine.destroy_session(session);
  engine.shutdown();
  std::cout << "ok\n";
  return 0;
}