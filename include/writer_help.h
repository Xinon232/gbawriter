#pragma once
// V4.0 Controls topics and Credits pages (Select menu), in the gbavocab /
// gbareader layout: a title, eight lines (one per list row), "#" lines are
// grey subheadings.
namespace writer {
constexpr int HELP_LINES = 8;
constexpr int HELP_TOPICS = 8; // 7 Controls topics, then Credits
struct HelpPage {
  const char *title;
  const char *lines[HELP_LINES];
};
const char *help_topic_name(int topic);
int help_topic_pages(int topic);
const HelpPage &help_page(int topic, int page);
constexpr int CREDITS_TOPIC = HELP_TOPICS - 1;
// The last Credits page opens Secret Settings with A.
bool help_secret_page(int topic, int page);
} // namespace writer
