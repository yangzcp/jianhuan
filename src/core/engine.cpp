#include "jianhuan/engine.h"

#include <rime_api.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <mutex>

namespace jianhuan {

struct Engine::Impl {
  RimeApi* api = nullptr;
  std::mutex mutex;
  bool initialized = false;
  std::string user_data_dir;
};

Engine& Engine::instance() {
  static Engine engine;
  return engine;
}

Engine::~Engine() {
  shutdown();
}

bool Engine::initialized() const {
  return impl_ && impl_->initialized;
}

bool Engine::initialize(const std::string& shared_data_dir,
                        const std::string& user_data_dir) {
  if (!impl_) {
    impl_ = new Impl();
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->initialized) {
    return true;
  }

  std::error_code ec;
  std::filesystem::create_directories(user_data_dir, ec);
#ifdef _WIN32
  const unsigned long pid = GetCurrentProcessId();
#else
  const unsigned long pid = getpid();
#endif
  const auto rime_user = std::filesystem::path(user_data_dir) /
                         ("proc_" + std::to_string(pid));
  std::filesystem::create_directories(rime_user, ec);
  std::filesystem::create_directories(rime_user / "log", ec);
  const auto src_phrase = std::filesystem::path(shared_data_dir) / "custom_phrase.txt";
  const auto dst_phrase = rime_user / "custom_phrase.txt";
  if (std::filesystem::exists(src_phrase) && !std::filesystem::exists(dst_phrase)) {
    std::filesystem::copy_file(src_phrase, dst_phrase, ec);
  }

  RimeApi* api = rime_get_api();
  if (!api) {
    return false;
  }

  RimeTraits traits = {};
  RIME_STRUCT_INIT(RimeTraits, traits);
  const std::string rime_user_str = rime_user.string();
  const std::string log_dir = (rime_user / "log").string();
  traits.shared_data_dir = shared_data_dir.c_str();
  traits.user_data_dir = rime_user_str.c_str();
  traits.distribution_name = "Jianhuan";
  traits.distribution_code_name = "jianhuan";
  traits.distribution_version = JIANHUAN_VERSION;
  traits.app_name = "rime.jianhuan";
  traits.min_log_level = 2;
  traits.log_dir = log_dir.c_str();

  api->setup(&traits);
  api->initialize(&traits);

  const auto prebuilt_dir = std::filesystem::path(shared_data_dir) / "build";
  const bool has_prebuilt = std::filesystem::exists(prebuilt_dir / "jianhuan_pinyin.table.bin");
  if (!has_prebuilt) {
    api->start_maintenance(false);
    api->join_maintenance_thread();
    const std::string pinyin_schema =
        (std::filesystem::path(shared_data_dir) / "jianhuan_pinyin.schema.yaml").string();
    const std::string double_schema =
        (std::filesystem::path(shared_data_dir) / "jianhuan_double_pinyin.schema.yaml").string();
    api->deploy_schema(pinyin_schema.c_str());
    api->deploy_schema(double_schema.c_str());
  }

  impl_->api = api;
  impl_->initialized = true;
  impl_->user_data_dir = user_data_dir;
  return true;
}

std::string Engine::user_data_dir() const {
  return impl_ ? impl_->user_data_dir : std::string{};
}

void Engine::shutdown() {
  if (!impl_) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->initialized && impl_->api) {
    impl_->api->finalize();
  }
  impl_->initialized = false;
  impl_->api = nullptr;
}

std::uint64_t Engine::create_session() {
  if (!initialized()) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return static_cast<std::uint64_t>(impl_->api->create_session());
}

void Engine::destroy_session(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->api->destroy_session(static_cast<RimeSessionId>(session));
}

bool Engine::process_key(std::uint64_t session, int keycode, int mask) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->process_key(static_cast<RimeSessionId>(session), keycode, mask) != False;
}

bool Engine::commit_composition(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->commit_composition(static_cast<RimeSessionId>(session)) != False;
}

void Engine::clear_composition(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->api->clear_composition(static_cast<RimeSessionId>(session));
}

bool Engine::get_commit(std::uint64_t session, std::string& text) {
  text.clear();
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  RimeCommit commit = {};
  RIME_STRUCT_INIT(RimeCommit, commit);
  if (!impl_->api->get_commit(static_cast<RimeSessionId>(session), &commit)) {
    return false;
  }
  if (commit.text) {
    text = commit.text;
  }
  impl_->api->free_commit(&commit);
  return true;
}

bool Engine::get_context(std::uint64_t session, Context& context) {
  context = Context{};
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  RimeContext ctx = {};
  RIME_STRUCT_INIT(RimeContext, ctx);
  if (!impl_->api->get_context(static_cast<RimeSessionId>(session), &ctx)) {
    return false;
  }

  if (ctx.composition.preedit) {
    context.preedit = ctx.composition.preedit;
  }
  context.cursor_pos = ctx.composition.cursor_pos;
  context.sel_start = ctx.composition.sel_start;
  context.sel_end = ctx.composition.sel_end;
  context.page_no = ctx.menu.page_no;
  context.page_size = ctx.menu.page_size;
  context.is_last_page = ctx.menu.is_last_page != False;
  context.highlighted = ctx.menu.highlighted_candidate_index;
  if (ctx.menu.select_keys) {
    context.select_keys = ctx.menu.select_keys;
  }
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    const RimeCandidate& candidate = ctx.menu.candidates[i];
    context.candidates.push_back({candidate.text ? candidate.text : "",
                                  candidate.comment ? candidate.comment : ""});
  }
  if (RIME_STRUCT_HAS_MEMBER(ctx, ctx.commit_text_preview) && ctx.commit_text_preview) {
    context.commit_preview = ctx.commit_text_preview;
  }

  impl_->api->free_context(&ctx);
  return true;
}

bool Engine::is_composing(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  RimeStatus status = {};
  RIME_STRUCT_INIT(RimeStatus, status);
  if (!impl_->api->get_status(static_cast<RimeSessionId>(session), &status)) {
    return false;
  }
  const bool composing = status.is_composing != False;
  impl_->api->free_status(&status);
  return composing;
}

std::string Engine::raw_input(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const char* input = impl_->api->get_input(static_cast<RimeSessionId>(session));
  return input ? std::string(input) : std::string{};
}

bool Engine::select_candidate(std::uint64_t session, std::size_t index) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->select_candidate(static_cast<RimeSessionId>(session), index) != False;
}

bool Engine::select_candidate_on_current_page(std::uint64_t session, std::size_t index) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->select_candidate_on_current_page(static_cast<RimeSessionId>(session), index) != False;
}

bool Engine::change_page(std::uint64_t session, bool backward) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->change_page(static_cast<RimeSessionId>(session), backward ? True : False) != False;
}

void Engine::set_option(std::uint64_t session, const std::string& option, bool value) {
  if (!initialized() || session == 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->api->set_option(static_cast<RimeSessionId>(session), option.c_str(), value ? True : False);
}

bool Engine::get_option(std::uint64_t session, const std::string& option) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->get_option(static_cast<RimeSessionId>(session), option.c_str()) != False;
}

bool Engine::select_schema(std::uint64_t session, const std::string& schema_id) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->select_schema(static_cast<RimeSessionId>(session), schema_id.c_str()) != False;
}

std::string Engine::current_schema(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  char buffer[256] = {};
  if (!impl_->api->get_current_schema(static_cast<RimeSessionId>(session), buffer, sizeof(buffer))) {
    return {};
  }
  return buffer;
}

}  // namespace jianhuan