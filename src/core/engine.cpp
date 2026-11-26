#include "jianhuan/engine.h"

#include <rime_api.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <fstream>

namespace jianhuan {

struct Engine::Impl {
  struct SessionRank {
    std::vector<int> perm;          // displayed index -> merged index
    int rime_count = 0;             // merged indices < rime_count are rime's
    std::vector<std::string> injected;
    std::string pending_commit;     // set when an injected candidate is chosen
  };
  RimeApi* api = nullptr;
  std::mutex mutex;
  bool initialized = false;
  std::string user_data_dir;
  std::unordered_map<std::string, std::vector<std::string>> predict;
  std::unordered_map<std::string, int> predict_len;
  bool predict_loaded = false;
  std::unordered_map<std::uint64_t, SessionRank> rank;
};

namespace {

void LoadPredict(Engine::Impl* impl, const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return;
  constexpr std::size_t kMinPrefix = 5;
  constexpr std::size_t kMaxPerPrefix = 6;
  std::string line;
  while (std::getline(file, line)) {
    const auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    const std::string text = line.substr(0, tab);
    const std::string code = line.substr(tab + 1);
    if (text.empty() || code.size() < kMinPrefix) continue;
    for (std::size_t len = kMinPrefix; len <= code.size(); ++len) {
      auto& bucket = impl->predict[code.substr(0, len)];
      if (std::find(bucket.begin(), bucket.end(), text) == bucket.end()) {
        bucket.push_back(text);
        impl->predict_len[text] = static_cast<int>(code.size());
      }
    }
  }
  for (auto& [prefix, bucket] : impl->predict) {
    std::stable_sort(bucket.begin(), bucket.end(), [&](const std::string& a, const std::string& b) {
      return impl->predict_len[a] < impl->predict_len[b];
    });
    if (bucket.size() > kMaxPerPrefix) bucket.resize(kMaxPerPrefix);
  }
  impl->predict_loaded = !impl->predict.empty();
}

}  // namespace

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
  LoadPredict(impl_, (std::filesystem::path(shared_data_dir) / "jianhuan_predict.txt").string());

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
  impl_->rank.erase(session);
  impl_->api->destroy_session(static_cast<RimeSessionId>(session));
}

bool Engine::process_key(std::uint64_t session, int keycode, int mask) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->api->process_key(static_cast<RimeSessionId>(session), keycode, mask) != False;
}

void Engine::clear_composition(std::uint64_t session) {
  if (!initialized() || session == 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->rank[session].pending_commit.clear();
  impl_->api->clear_composition(static_cast<RimeSessionId>(session));
}

bool Engine::get_commit(std::uint64_t session, std::string& text) {
  text.clear();
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  auto& pending = impl_->rank[session].pending_commit;
  if (!pending.empty()) {
    text = pending;
    pending.clear();
    return true;
  }
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
  context.page_no = ctx.menu.page_no;
  context.page_size = ctx.menu.page_size;
  context.is_last_page = ctx.menu.is_last_page != False;
  context.highlighted = ctx.menu.highlighted_candidate_index;
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    const RimeCandidate& candidate = ctx.menu.candidates[i];
    context.candidates.push_back({candidate.text ? candidate.text : "",
                                  candidate.comment ? candidate.comment : ""});
  }

  const int n = static_cast<int>(context.candidates.size());
  std::vector<int> lead;  // merged indices placed before rime's native order
  std::vector<std::string> injected;
  const char* raw = impl_->api->get_input(static_cast<RimeSessionId>(session));
  const std::string input = raw ? raw : "";
  if (context.page_no == 0 && impl_->predict_loaded && input.size() >= 5) {
    const auto pit = impl_->predict.find(input);
    if (pit != impl_->predict.end() && !pit->second.empty()) {
      const std::string& base = pit->second.front();
      const auto at = std::find_if(context.candidates.begin(), context.candidates.end(),
                                   [&](const Candidate& c) { return c.text == base; });
      if (at != context.candidates.end()) {
        lead.push_back(static_cast<int>(at - context.candidates.begin()));
      } else {
        for (const auto& text : pit->second) {
          const bool present = std::any_of(context.candidates.begin(), context.candidates.end(),
                                           [&](const Candidate& c) { return c.text == text; });
          if (!present) {
            lead.push_back(static_cast<int>(context.candidates.size()));
            context.candidates.push_back({text, ""});
            injected.push_back(text);
          }
        }
      }
    }
  }
  std::vector<int> order = lead;
  for (int i = 0; i < n; ++i) {
    if (std::find(lead.begin(), lead.end(), i) == lead.end()) order.push_back(i);
  }
  std::vector<Candidate> ranked;
  ranked.reserve(order.size());
  for (int idx : order) ranked.push_back(context.candidates[idx]);
  const int page = context.page_size > 0 ? context.page_size : 5;
  if (static_cast<int>(ranked.size()) > page) {
    ranked.resize(page);
    order.resize(page);
  }
  const auto hi = std::find(order.begin(), order.end(), context.highlighted);
  context.highlighted = hi != order.end() ? static_cast<int>(hi - order.begin()) : 0;
  context.candidates = std::move(ranked);

  auto& r = impl_->rank[session];
  r.perm = std::move(order);
  r.rime_count = n;
  r.injected = std::move(injected);
  r.pending_commit.clear();

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

bool Engine::select_candidate_on_current_page(std::uint64_t session, std::size_t index) {
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto& r = impl_->rank[session];
  if (r.perm.empty()) {
    return impl_->api->select_candidate_on_current_page(static_cast<RimeSessionId>(session), index) != False;
  }
  const int merged = index < r.perm.size() ? r.perm[index] : static_cast<int>(index);
  if (merged < r.rime_count) {
    return impl_->api->select_candidate_on_current_page(static_cast<RimeSessionId>(session), merged) != False;
  }
  const int j = merged - r.rime_count;
  if (j >= 0 && j < static_cast<int>(r.injected.size())) {
    impl_->rank[session].pending_commit = r.injected[j];
    impl_->api->clear_composition(static_cast<RimeSessionId>(session));
    return true;
  }
  return false;
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