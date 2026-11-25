#include "jianhuan/engine.h"
#include "jianhuan/settings.h"

#include <iostream>
#include <string>

namespace {

int keysym_for_char(char c) {
  return static_cast<unsigned char>(c);
}

void print_context(const jianhuan::Context& context) {
  if (!context.preedit.empty()) {
    std::cout << "[" << context.preedit << "]";
  }
  for (std::size_t i = 0; i < context.candidates.size(); ++i) {
    std::cout << " " << (i + 1) << "." << context.candidates[i].text;
    if (!context.candidates[i].comment.empty()) {
      std::cout << "(" << context.candidates[i].comment << ")";
    }
  }
  std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: jianhuan_console <shared_data_dir> <user_data_dir> [schema_id]\n";
    return 1;
  }

  auto& engine = jianhuan::Engine::instance();
  if (!engine.initialize(argv[1], argv[2])) {
    std::cerr << "failed to initialize engine\n";
    return 1;
  }

  const std::uint64_t session = engine.create_session();
  if (session == 0) {
    std::cerr << "failed to create session\n";
    return 1;
  }

  if (argc >= 4) {
    engine.select_schema(session, argv[3]);
  } else {
    jianhuan::Settings settings;
    jianhuan::load_settings(argv[2], settings);
    engine.select_schema(session, settings.schema_id);
    engine.set_option(session, "ascii_mode", settings.ascii_mode);
    engine.set_option(session, "full_shape", settings.full_shape);
    engine.set_option(session, "ascii_punct", settings.ascii_punct);
  }

  std::cout << "schema: " << engine.current_schema(session) << "\n";
  std::cout << "type pinyin, space to select, /schema pinyin|double, /ascii, /punct, /save, /quit\n";

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "/quit") {
      break;
    }
    if (line == "/schema pinyin") {
      engine.select_schema(session, "jianhuan_pinyin");
      std::cout << "schema: " << engine.current_schema(session) << "\n";
      continue;
    }
    if (line == "/schema double") {
      engine.select_schema(session, "jianhuan_double_pinyin");
      std::cout << "schema: " << engine.current_schema(session) << "\n";
      continue;
    }
    if (line == "/ascii") {
      const bool value = !engine.get_option(session, "ascii_mode");
      engine.set_option(session, "ascii_mode", value);
      std::cout << "ascii_mode: " << value << "\n";
      continue;
    }
    if (line == "/punct") {
      const bool value = !engine.get_option(session, "ascii_punct");
      engine.set_option(session, "ascii_punct", value);
      std::cout << "ascii_punct: " << value << "\n";
      continue;
    }
    if (line == "/save") {
      jianhuan::Settings settings;
      settings.schema_id = engine.current_schema(session);
      settings.ascii_mode = engine.get_option(session, "ascii_mode");
      settings.full_shape = engine.get_option(session, "full_shape");
      settings.ascii_punct = engine.get_option(session, "ascii_punct");
      jianhuan::save_settings(argv[2], settings);
      std::cout << "saved\n";
      continue;
    }
    if (line == "/page+") {
      engine.change_page(session, false);
    } else if (line == "/page-") {
      engine.change_page(session, true);
    } else if (line == "/clear") {
      engine.clear_composition(session);
    } else if (line.size() == 2 && line[0] == '/' && line[1] >= '1' && line[1] <= '9') {
      engine.select_candidate_on_current_page(session, static_cast<std::size_t>(line[1] - '1'));
    } else {
      for (char c : line) {
        engine.process_key(session, keysym_for_char(c), 0);
      }
      engine.process_key(session, 0x20, 0);
    }

    std::string commit;
    if (engine.get_commit(session, commit)) {
      std::cout << "commit: " << commit << "\n";
    }
    jianhuan::Context context;
    if (engine.get_context(session, context)) {
      print_context(context);
    }
  }

  engine.destroy_session(session);
  engine.shutdown();
  return 0;
}