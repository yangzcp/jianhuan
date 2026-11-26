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
#include <cmath>
#include <climits>
#include <algorithm>
#include <fstream>

namespace jianhuan {

struct Engine::Impl {
  RimeApi* api = nullptr;
  std::mutex mutex;
  bool initialized = false;
  std::string user_data_dir;
  std::unordered_map<char32_t, std::uint32_t> uni;
  std::unordered_map<std::uint64_t, std::uint32_t> bi;
  std::uint32_t vocab = 0;
  bool lm_loaded = false;
  std::unordered_map<std::string, std::vector<std::string>> predict;
  bool predict_loaded = false;
  std::unordered_map<std::uint64_t, std::vector<int>> perm;
  std::unordered_map<std::uint64_t, int> rime_count;
  std::unordered_map<std::uint64_t, std::vector<std::string>> inject;
  std::unordered_map<std::uint64_t, std::string> pending_commit;
};

namespace {

std::u32string DecodeUtf8(const std::string& s) {
  std::u32string out;
  for (std::size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    char32_t cp = 0;
    int n = 1;
    if (c < 0x80) { cp = c; n = 1; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
    else { cp = c & 0x07; n = 4; }
    for (int k = 1; k < n && i + k < s.size(); k++) {
      cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    }
    out.push_back(cp);
    i += n;
  }
  return out;
}

void LoadLm(Engine::Impl* impl, const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return;
  std::string line;
  while (std::getline(file, line)) {
    const auto t1 = line.find('\t');
    if (t1 == std::string::npos) continue;
    const auto t2 = line.find('\t', t1 + 1);
    if (t2 == std::string::npos) continue;
    const std::string type = line.substr(0, t1);
    const std::string key = line.substr(t1 + 1, t2 - t1 - 1);
    const std::uint32_t count = static_cast<std::uint32_t>(std::stoul(line.substr(t2 + 1)));
    const std::u32string chars = DecodeUtf8(key);
    if (type == "U" && chars.size() == 1) {
      impl->uni[chars[0]] = count;
    } else if (type == "B" && chars.size() == 2) {
      impl->bi[(static_cast<std::uint64_t>(chars[0]) << 32) | chars[1]] = count;
    }
  }
  impl->vocab = static_cast<std::uint32_t>(impl->uni.size());
  impl->lm_loaded = impl->vocab > 0;
}

double LmScore(Engine::Impl* impl, const std::string& text) {
  const std::u32string chars = DecodeUtf8(text);
  if (chars.empty()) return -1e9;
  const char32_t bos = 0x02;
  double sum = 0;
  char32_t prev = bos;
  for (char32_t c : chars) {
    const auto bit = impl->bi.find((static_cast<std::uint64_t>(prev) << 32) | c);
    const auto uit = impl->uni.find(prev);
    const double num = (bit != impl->bi.end() ? bit->second : 0) + 1.0;
    const double den = (uit != impl->uni.end() ? uit->second : 0) + impl->vocab + 1.0;
    sum += std::log(num / den);
    prev = c;
  }
  return sum / static_cast<double>(chars.size());
}

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
      if (bucket.size() >= kMaxPerPrefix) continue;
      if (std::find(bucket.begin(), bucket.end(), text) == bucket.end()) {
        bucket.push_back(text);
      }
    }
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
  LoadLm(impl_, (std::filesystem::path(shared_data_dir) / "jianhuan_lm.txt").string());
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
  impl_->perm.erase(session);
  impl_->rime_count.erase(session);
  impl_->inject.erase(session);
  impl_->pending_commit.erase(session);
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
  impl_->pending_commit.erase(session);
  impl_->api->clear_composition(static_cast<RimeSessionId>(session));
}

bool Engine::get_commit(std::uint64_t session, std::string& text) {
  text.clear();
  if (!initialized() || session == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto pc = impl_->pending_commit.find(session);
  if (pc != impl_->pending_commit.end()) {
    text = pc->second;
    impl_->pending_commit.erase(pc);
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

  const int n = static_cast<int>(context.candidates.size());
  std::vector<std::string> injected;
  const char* raw = impl_->api->get_input(static_cast<RimeSessionId>(session));
  const std::string input = raw ? raw : "";
  if (impl_->predict_loaded && input.size() >= 5) {
    const auto pit = impl_->predict.find(input);
    if (pit != impl_->predict.end()) {
      for (const auto& text : pit->second) {
        bool present = false;
        for (const auto& c : context.candidates) {
          if (c.text == text) { present = true; break; }
        }
        if (!present) {
          context.candidates.push_back({text, ""});
          injected.push_back(text);
        }
      }
    }
  }
  const int m = static_cast<int>(context.candidates.size());
  std::vector<int> order(m);
  for (int i = 0; i < m; ++i) order[i] = i;
  auto score = [&](int idx) {
    if (idx >= n) return 100.0 - static_cast<double>(idx - n);
    return impl_->lm_loaded ? LmScore(impl_, context.candidates[idx].text) : 0.0;
  };
  if (m >= 2) {
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
      return score(a) > score(b);
    });
  }
  std::vector<Candidate> reordered;
  reordered.reserve(m);
  std::vector<int> inverse(m, 0);
  for (int d = 0; d < m; ++d) {
    reordered.push_back(context.candidates[order[d]]);
    inverse[order[d]] = d;
  }
  context.candidates = std::move(reordered);
  if (context.highlighted >= 0 && context.highlighted < n) {
    context.highlighted = inverse[context.highlighted];
  }
  const int page = context.page_size > 0 ? context.page_size : 5;
  if (static_cast<int>(context.candidates.size()) > page) {
    context.candidates.resize(page);
    order.resize(page);
    if (context.highlighted >= page) context.highlighted = 0;
  }
  impl_->perm[session] = std::move(order);
  impl_->rime_count[session] = n;
  impl_->inject[session] = std::move(injected);

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

void Engine::rerank_texts(std::vector<std::string>& texts) {
  if (!impl_ || !impl_->lm_loaded || texts.size() < 2) {
    return;
  }
  std::stable_sort(texts.begin(), texts.end(), [this](const std::string& a, const std::string& b) {
    return LmScore(impl_, a) > LmScore(impl_, b);
  });
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
  int merged = static_cast<int>(index);
  const auto it = impl_->perm.find(session);
  if (it != impl_->perm.end() && index < it->second.size()) {
    merged = it->second[index];
  }
  int n = INT_MAX;
  const auto nit = impl_->rime_count.find(session);
  if (nit != impl_->rime_count.end()) n = nit->second;
  if (merged < n) {
    return impl_->api->select_candidate_on_current_page(static_cast<RimeSessionId>(session), merged) != False;
  }
  const auto& inj = impl_->inject[session];
  const int j = merged - n;
  if (j >= 0 && j < static_cast<int>(inj.size())) {
    impl_->pending_commit[session] = inj[j];
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