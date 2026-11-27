#include "jianhuan/engine.h"

#include <rime_api.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
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
  std::unordered_map<std::string, double> freq;
  std::unordered_map<std::string, std::uint32_t> user_freq;
  std::string user_freq_path;
  std::unordered_map<std::string, std::string> code;
  std::unordered_map<std::string, std::string> revcode;
  std::unordered_map<char32_t, std::uint32_t> uni;
  std::unordered_map<std::uint64_t, std::uint32_t> bi;
  std::uint32_t vocab = 0;
  bool lm_loaded = false;
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

void LoadFreq(Engine::Impl* impl, const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return;
  std::string line;
  while (std::getline(file, line)) {
    const auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    const std::string text = line.substr(0, tab);
    if (text.empty()) continue;
    impl->freq[text] = std::stod(line.substr(tab + 1));
  }
}

void LoadCode(Engine::Impl* impl, const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return;
  std::string line;
  while (std::getline(file, line)) {
    const auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    impl->code[line.substr(0, tab)] = line.substr(tab + 1);
  }
}

void LoadRevCode(Engine::Impl* impl, const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return;
  std::string line;
  while (std::getline(file, line)) {
    const auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    impl->revcode[line.substr(0, tab)] = line.substr(tab + 1);
  }
}

// Personalization: append-only log of committed texts; counts loaded at init.
void LoadUserFreq(Engine::Impl* impl, const std::string& path) {
  impl->user_freq_path = path;
  std::ifstream file(path);
  if (!file.is_open()) return;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) impl->user_freq[line]++;
  }
}

void RecordUserFreq(Engine::Impl* impl, const std::string& text) {
  if (text.empty() || impl->user_freq_path.empty()) return;
  impl->user_freq[text]++;
  std::ofstream file(impl->user_freq_path, std::ios::app);
  if (file.is_open()) file << text << "\n";
}

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
    const std::u32string chars = DecodeUtf8(line.substr(t1 + 1, t2 - t1 - 1));
    const std::uint32_t count = static_cast<std::uint32_t>(std::stoul(line.substr(t2 + 1)));
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
  if (chars.empty()) return -10.0;
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

bool AdjacentKey(char a, char b) {
  static const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  int ra = -1, ca = -1, rb = -1, cb = -1;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; rows[r][c]; c++) {
      if (rows[r][c] == a) { ra = r; ca = c; }
      if (rows[r][c] == b) { rb = r; cb = c; }
    }
  }
  if (ra < 0 || rb < 0) return false;
  const int dr = std::abs(ra - rb), dc = std::abs(ca - cb);
  return (dr == 0 && dc == 1) || (dr == 1 && dc <= 1);
}

// Damerau-Levenshtein with keyboard-adjacency-weighted substitution.
double AdjEditDistance(const std::string& a, const std::string& b) {
  const std::size_t n = a.size(), m = b.size();
  std::vector<std::vector<double>> d(n + 1, std::vector<double>(m + 1, 0));
  for (std::size_t i = 0; i <= n; i++) d[i][0] = i;
  for (std::size_t j = 0; j <= m; j++) d[0][j] = j;
  for (std::size_t i = 1; i <= n; i++) {
    for (std::size_t j = 1; j <= m; j++) {
      const double sub = AdjacentKey(a[i - 1], b[j - 1]) ? 0.5 : 1.0;
      double best = std::min({d[i - 1][j] + 1.0, d[i][j - 1] + 1.0, d[i - 1][j - 1] + sub});
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
        best = std::min(best, d[i - 2][j - 2] + 0.5);
      }
      d[i][j] = best;
    }
  }
  return d[n][m];
}

// Engine-side fuzzy-pinyin + keyboard-typo expansion (the vendored librime
// lacks fuzz/correction build support). Generates alternate input spellings.
std::vector<std::string> TypoFuzzyVariants(const std::string& in) {
  std::vector<std::string> out;
  const auto push = [&](std::string v) {
    if (v != in && v.size() >= 2 && out.size() < 80 &&
        std::find(out.begin(), out.end(), v) == out.end()) {
      out.push_back(std::move(v));
    }
  };
  const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const auto neighbors = [&](char c) {
    std::string r;
    for (const char* row : rows) {
      for (int i = 0; row[i]; i++) {
        if (row[i] == c) {
          if (i > 0) r += row[i - 1];
          if (row[i + 1]) r += row[i + 1];
        }
      }
    }
    return r;
  };
  for (std::size_t i = 0; i < in.size(); i++) {
    push(in.substr(0, i) + in.substr(i + 1));
    if (i + 1 < in.size()) {
      std::string v = in;
      std::swap(v[i], v[i + 1]);
      push(v);
    }
    for (char k : neighbors(in[i])) {
      std::string v = in;
      v[i] = k;
      push(v);
    }
    if (in[i] == 'n' || in[i] == 'l') {
      std::string v = in;
      v[i] = (in[i] == 'n') ? 'l' : 'n';
      push(v);
    }
    if (in[i] == 's' || in[i] == 'c' || in[i] == 'z') {
      push(in.substr(0, i + 1) + "h" + in.substr(i + 1));
    }
  }
  const auto repl = [&](const std::string& a, const std::string& b) {
    if (in.size() >= a.size() && in.compare(in.size() - a.size(), a.size(), a) == 0) {
      push(in.substr(0, in.size() - a.size()) + b);
    }
  };
  repl("eng", "en"); repl("en", "eng");
  repl("ing", "in"); repl("in", "ing");
  repl("ang", "an"); repl("an", "ang");
  return out;
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
  LoadFreq(impl_, (std::filesystem::path(shared_data_dir) / "jianhuan_freq.txt").string());
  LoadCode(impl_, (std::filesystem::path(shared_data_dir) / "jianhuan_code.txt").string());
  LoadRevCode(impl_, (std::filesystem::path(shared_data_dir) / "jianhuan_revcode.txt").string());
  LoadLm(impl_, (std::filesystem::path(shared_data_dir) / "jianhuan_lm.txt").string());
  LoadUserFreq(impl_, (std::filesystem::path(user_data_dir) / "jianhuan_user_freq.txt").string());

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
    RecordUserFreq(impl_, text);
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
  RecordUserFreq(impl_, text);
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
  std::vector<std::string> injected;
  const char* raw = impl_->api->get_input(static_cast<RimeSessionId>(session));
  const std::string input = raw ? raw : "";
  bool full_match = false;
  std::vector<std::string> typo_injected;
  if (context.page_no == 0 && impl_->predict_loaded && input.size() >= 5) {
    const auto pit = impl_->predict.find(input);
    if (pit != impl_->predict.end() && !pit->second.empty()) {
      const int ilen = static_cast<int>(input.size());
      for (const auto& text : pit->second) {
        if (impl_->predict_len[text] != ilen) continue;
        if (std::any_of(context.candidates.begin(), context.candidates.end(),
                        [&](const Candidate& c) { return c.text == text; })) {
          full_match = true;
          break;
        }
      }
      // Full code already produced natively: trust frequency order, no inject.
      if (!full_match) {
        for (const auto& text : pit->second) {
          const bool present = std::any_of(context.candidates.begin(), context.candidates.end(),
                                           [&](const Candidate& c) { return c.text == text; });
          if (!present) {
            context.candidates.push_back({text, ""});
            injected.push_back(text);
          }
        }
      }
    }
  }
  // Fuzzy-pinyin / keyboard-typo tolerance: look up alternate spellings and
  // surface their best word; unified score ranks them by frequency+distance.
  if (context.page_no == 0 && !full_match && input.size() >= 2 &&
      impl_->predict.find(input) == impl_->predict.end()) {
    for (const auto& variant : TypoFuzzyVariants(input)) {
      if (injected.size() >= 8) break;
      const auto rit = impl_->revcode.find(variant);
      if (rit == impl_->revcode.end()) continue;
      const std::string& text = rit->second;
      const bool present = std::any_of(context.candidates.begin(), context.candidates.end(),
                                       [&](const Candidate& c) { return c.text == text; });
      if (!present) {
        context.candidates.push_back({text, ""});
        injected.push_back(text);
        typo_injected.push_back(text);
      }
    }
  }
  const int m = static_cast<int>(context.candidates.size());
  // Unified score: dictionary frequency base, userdb-learned protection,
  // native/exact-match bonus, shorter-code tiebreak. Highlight = rank #1.
  const auto score = [&](int idx) {
    const std::string& text = context.candidates[idx].text;
    const bool native = idx < n;
    const auto fit = impl_->freq.find(text);
    const double freq = fit != impl_->freq.end() ? fit->second : 0.0;
    double s = std::log10(freq + 1.0);
    // Personalization: repeatedly committed candidates rise over time.
    const auto ufit = impl_->user_freq.find(text);
    if (ufit != impl_->user_freq.end()) s += 2.0 * std::log10(ufit->second + 1.0);
    if (native) s += 0.25;
    // Emoji candidates carry no dictionary frequency; keep them visible.
    for (unsigned char ch : text) {
      if (ch >= 0xF0) { s += 5.0; break; }
    }
    if (native && text == input) {
      // Raw letter fallback (no dictionary frequency) sinks to the bottom;
      // a real English word exactly matching the input leads.
      s += freq > 0.0 ? 6.0 : -100.0;
    }
    const auto lit = impl_->predict_len.find(text);
    const int clen = lit != impl_->predict_len.end() ? lit->second : 0;
    s -= 0.001 * clen;
    // Self-trained char-bigram LM as a secondary signal (tie-break among
    // equal-frequency candidates), never overriding dictionary frequency.
    if (impl_->lm_loaded) s += 0.5 * LmScore(impl_, text);
    // Keyboard-adjacency-weighted edit distance penalizes misspelled codes,
    // so adjacent-key typos rank above random ones.
    const auto cit = impl_->code.find(text);
    if (cit != impl_->code.end() && cit->second != input &&
        std::find(typo_injected.begin(), typo_injected.end(), text) != typo_injected.end()) {
      s -= 0.3 * AdjEditDistance(input, cit->second);
    }
    // Demote native candidates whose code neither equals nor prefix-matches the
    // input (stray correction artifacts), unless they are a predicted completion.
    if (native && cit != impl_->code.end() && cit->second != input) {
      const std::string& c = cit->second;
      const bool prefix = c.compare(0, input.size(), input) == 0 ||
                          input.compare(0, c.size(), c) == 0;
      const auto b = impl_->predict.find(input);
      const bool in_bucket = b != impl_->predict.end() &&
                             std::find(b->second.begin(), b->second.end(), text) != b->second.end();
      if (!prefix && !in_bucket) s -= 3.0;
    }
    // Prefer candidates whose full code covers the whole input (exact or a
    // completion), so a shorter prefix-word cannot outrank the full match.
    if (input.size() >= 5 && clen >= static_cast<int>(input.size())) {
      const auto b = impl_->predict.find(input);
      if (b != impl_->predict.end() &&
          std::find(b->second.begin(), b->second.end(), text) != b->second.end()) {
        s += 6.0;
      }
    }
    return s;
  };
  std::vector<int> order(m);
  for (int i = 0; i < m; ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return score(a) > score(b); });
  std::vector<Candidate> ranked;
  ranked.reserve(m);
  for (int idx : order) ranked.push_back(context.candidates[idx]);
  const int page = context.page_size > 0 ? context.page_size : 5;
  if (static_cast<int>(ranked.size()) > page) {
    ranked.resize(page);
    order.resize(page);
  }
  context.highlighted = 0;
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
  const char* raw = impl_->api->get_input(static_cast<RimeSessionId>(session));
  const std::string prior = raw ? raw : "";
  const auto& r = impl_->rank[session];
  std::string selected;
  bool ok = false;
  if (r.perm.empty()) {
    ok = impl_->api->select_candidate_on_current_page(static_cast<RimeSessionId>(session), index) != False;
  } else {
    const int merged = index < r.perm.size() ? r.perm[index] : static_cast<int>(index);
    if (merged < r.rime_count) {
      RimeContext ctx = {};
      RIME_STRUCT_INIT(RimeContext, ctx);
      if (impl_->api->get_context(static_cast<RimeSessionId>(session), &ctx)) {
        if (merged < ctx.menu.num_candidates && ctx.menu.candidates[merged].text) {
          selected = ctx.menu.candidates[merged].text;
        }
        impl_->api->free_context(&ctx);
      }
      ok = impl_->api->select_candidate_on_current_page(static_cast<RimeSessionId>(session), merged) != False;
    } else {
      const int j = merged - r.rime_count;
      if (j >= 0 && j < static_cast<int>(r.injected.size())) {
        selected = r.injected[j];
        impl_->rank[session].pending_commit = selected;
        impl_->api->clear_composition(static_cast<RimeSessionId>(session));
        ok = true;
      }
    }
  }
  // Industry partial-selection: keep the unselected tail of the input composing.
  if (ok && !selected.empty()) {
    const auto cit = impl_->code.find(selected);
    if (cit != impl_->code.end() && prior.size() > cit->second.size() &&
        prior.compare(0, cit->second.size(), cit->second) == 0) {
      RimeStatus st = {};
      RIME_STRUCT_INIT(RimeStatus, st);
      bool composing = false;
      if (impl_->api->get_status(static_cast<RimeSessionId>(session), &st)) {
        composing = st.is_composing != False;
        impl_->api->free_status(&st);
      }
      if (!composing) {
        for (char c : prior.substr(cit->second.size())) {
          impl_->api->process_key(static_cast<RimeSessionId>(session), static_cast<unsigned char>(c), 0);
        }
      }
    }
  }
  return ok;
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