#include <curl/curl.h>
#include <filesystem>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <mpv/client.h>
#include <nlohmann/json.hpp>
#include <print>
#include <sstream>

using namespace ftxui;
namespace fs = std::filesystem;

struct Lyrics {
  double time_sec;
  std::string lyrics;
};

struct PlayState {
  std::vector<std::string> file_paths;
  std::vector<std::string> titles;

  std::string current_playing;
  std::string current_artist;

  int current_index = 0;
  double position_sec = 0.0f;
  double total_sec = 0.0f;
  bool is_playing = false;

  std::vector<Lyrics> lyrics;
  int lyrics_index = 0;
  bool has_lyrics = false;
};

static PlayState state;

bool init_music(fs::path path, mpv_handle *mpv) {
  if (!fs::exists(path)) {
    std::println(stderr, "{} does not exits", fs::path(path).string());
    return false;
  }

  for (const auto &entry : fs::directory_iterator(path)) {
    if (entry.is_regular_file() and entry.path().extension() == ".mp3") {
      state.file_paths.push_back(fs::absolute(entry).string());
      state.titles.push_back(entry.path().filename().string());
    }
  }

  if (state.file_paths.empty()) {
    std::println(stderr, "No mp3 files found");
    return false;
  }

  return true;
}

bool init_mpv(mpv_handle *mpv) {
  if (!mpv) {
    std::println(stderr, "Failed to create mpv");
    return false;
  }

  mpv_set_option_string(mpv, "vo", "no");
  mpv_set_option_string(mpv, "idle", "yes");

  if (mpv_initialize(mpv) < 0) {
    std::println(stderr, "Failed to initiliaze mpv");
    mpv_destroy(mpv);
    return false;
  }
  return true;
}

void next(mpv_handle *mpv) {
  state.current_index++;
  if (state.current_index >= state.file_paths.size())
    state.current_index = 0;

  const char *cmd[] = {
      "loadfile", state.file_paths.at(state.current_index).c_str(), nullptr};
  mpv_command(mpv, cmd);
}

void prev(mpv_handle *mpv) {
  state.current_index--;
  if (state.current_index < 0)
    state.current_index = state.file_paths.size() - 1;

  const char *cmd[] = {
      "loadfile", state.file_paths.at(state.current_index).c_str(), nullptr};
  mpv_command(mpv, cmd);
}

void toggle_pause(mpv_handle *mpv) {
  const char *cmd[] = {"cycle", "pause", nullptr};
  mpv_command(mpv, cmd);
  state.is_playing = !state.is_playing;
}

void play(mpv_handle *mpv) {
  if (state.current_playing !=
      fs::path(state.file_paths.at(state.current_index)).filename().string()) {
    const char *cmd[] = {
        "loadfile", state.file_paths.at(state.current_index).c_str(), nullptr};
    mpv_command(mpv, cmd);
  }
}

void seek(mpv_handle *mpv, bool backward = false) {
  const char *cmd[] = {"seek", backward ? "-10" : "10", "relative", nullptr};
  mpv_command(mpv, cmd);
}

std::string format_time(double total_sec) {
  int min = static_cast<int>(total_sec) / 60;
  int sec = static_cast<int>(total_sec) % 60;
  return std::format("{}:{:02}", min, sec);
}

size_t cb(void *ptr, size_t size, size_t nmemb, void *buf) {
  *(std::string *)buf += std::string((char *)ptr, size * nmemb);
  return size * nmemb;
}

void parse_lyrics(const nlohmann::json &json) {
  state.lyrics.clear();
  auto synced_lyrics = json["syncedLyrics"];
  if (synced_lyrics.is_null()) {
    state.has_lyrics = false;
    return;
  }

  std::istringstream stream(synced_lyrics.get<std::string>());
  std::string line;
  while (std::getline(stream, line)) {
    if (line.size() < 10 || line[0] != '[')
      continue;

    int min = std::stoi(line.substr(1, 2));
    double sec = std::stod(line.substr(4, 5));
    double time_sec = min * 60.0 + sec;

    auto text = line.substr(10);
    if (text.starts_with(" "))
      text = text.substr(1);

    state.lyrics.push_back({time_sec, text});
  }
}

void get_lyrics() {
  std::string buf;
  CURL *curl = curl_easy_init();

  char *artist = curl_easy_escape(curl, state.current_artist.c_str(), 0);
  auto track = fs::path(state.current_playing).stem().string();
  char *song = curl_easy_escape(curl, track.c_str(), 0);

  curl_easy_setopt(curl, CURLOPT_USERAGENT, "music_player/1.0");
  curl_easy_setopt(
      curl, CURLOPT_URL,
      std::format("https://lrclib.net/api/get?artist_name={}&track_name={}",
                  artist, song)
          .c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

  curl_free(artist);
  curl_free(song);

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK || buf.empty()) {
    state.lyrics.clear();
    state.has_lyrics = false;
    return;
  }

  if (http_code >= 400) {
    state.lyrics.clear();
    state.has_lyrics = false;
    return;
  }

  try {
    auto json = nlohmann::json::parse(buf);
    parse_lyrics(json);
  } catch (const nlohmann::json::parse_error &e) {
    state.lyrics.clear();
    state.has_lyrics = false;
  }
}

void update_lyrics_index() {
  state.lyrics_index = 0;

  for (int i = 0; i < static_cast<int>(state.lyrics.size()); ++i) {
    if (state.position_sec >= state.lyrics[i].time_sec)
      state.lyrics_index = i;
    else
      break;
  }
}

int main(int argc, char **argv) {
  mpv_handle *mpv = mpv_create();

  int ok = init_mpv(mpv);
  if (!ok)
    return 1;

  if (argc < 2) {
    ok = init_music("music", mpv);
  } else {
    auto dir = fs::path(argv[1]);
    if (fs::exists(dir)) {
      ok = init_music(argv[1], mpv);
    }
  }

  if (!ok)
    return 1;

  App screen = App::Fullscreen();
  auto menu = Menu(&state.titles, &state.current_index) | frame;
  auto prev_btn = Button("", [&] { prev(mpv); }, ButtonOption::Ascii());

  ButtonOption play_opts;
  play_opts.transform = [&](const EntryState &s) {
    return text(state.is_playing ? "  " : "  ");
  };
  auto play_btn = Button("", [&] { toggle_pause(mpv); }, play_opts);

  auto next_btn = Button("", [&] { next(mpv); }, ButtonOption::Ascii());

  auto controls = Container::Horizontal({prev_btn, play_btn, next_btn});
  auto renderer = Renderer(controls, [&] {
    float ratio =
        state.total_sec > 0 ? state.position_sec / state.total_sec : 0.0f;

    std::vector<Element> lyric_children;
    lyric_children.reserve(state.lyrics.size());
    for (int i = 0; i < state.lyrics.size(); i++) {
      if (i == state.lyrics_index)
        lyric_children.push_back(text(state.lyrics[i].lyrics) | bold | focus |
                                 color(Color::Green));
      else
        lyric_children.push_back(text(state.lyrics[i].lyrics) | dim);
    }

    // clang-format off
    return vbox(
      separator(),
      state.lyrics.empty()
      ? vbox(
        filler(), 
        text("No lyrics found") | dim | center, filler()
      ) | flex
      : vbox(std::move(lyric_children)) | frame | flex,
      separator(),
      state.current_playing.empty()
      ? text("Nothing is playing") | center
      : vbox(
        text(state.current_playing) | bold | center | color(Color::Cyan),
        text(state.current_artist) | dim | center | color(Color::White)
      ),
      hbox(
        controls->Render(),
        gauge(ratio),
        hbox(
          text(format_time(state.position_sec)),
          text("/"),
          text(format_time(state.total_sec))
        )
      )
    );
  });
  // clang-format on

  auto container = Container::Vertical({menu, renderer | flex});
  auto app = CatchEvent(container, [&screen, &mpv](Event e) {
    if (e == Event::q) {
      screen.Exit();
      return false;
    }

    if (e == Event::Character(' ')) {
      toggle_pause(mpv);
      return true;
    }
    if (e == Event::n) {
      next(mpv);
      return true;
    }
    if (e == Event::CtrlN) {
      prev(mpv);
      return true;
    }

    if (e == Event::Return) {
      play(mpv);
      return true;
    }

    if (e == Event::l or e == Event::ArrowRight) {
      seek(mpv);
      return true;
    }
    if (e == Event::h or e == Event::ArrowLeft) {
      seek(mpv, true);
      return true;
    }

    return false;
  });

  Loop loop(&screen, app);

  while (!loop.HasQuitted()) {
    loop.RunOnce();
    mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &state.position_sec);

    update_lyrics_index();
    screen.RequestAnimationFrame();

    mpv_event *ev;

    while ((ev = mpv_wait_event(mpv, 0))->event_id != MPV_EVENT_NONE) {
      if (ev->event_id == MPV_EVENT_FILE_LOADED) {
        mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &state.total_sec);

        char *tmp_title = mpv_get_property_string(mpv, "media-title");
        state.current_playing = tmp_title;
        mpv_free(tmp_title);

        char *tmp_artist =
            mpv_get_property_string(mpv, "metadata/by-key/Artist");
        state.current_artist = tmp_artist;
        mpv_free(tmp_artist);

        int paused = 0;
        mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &paused);
        state.is_playing = !paused;

        state.lyrics_index = 0;
        get_lyrics();
      }

      if (ev->event_id == MPV_EVENT_END_FILE) {
        auto *end = static_cast<mpv_event_end_file *>(ev->data);
        if (end->reason == MPV_END_FILE_REASON_EOF)
          next(mpv);
      }
    }
  }

  mpv_terminate_destroy(mpv);
  return 0;
}
