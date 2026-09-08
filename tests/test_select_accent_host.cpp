#include "writer_core.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>
#ifdef SELECT_ACCENT_VOCAB
#include "entry_editor.h"
#else
#include "writer_app.h"
#include <filesystem>
#include <unistd.h>
#endif
#include "select_accent_matrix.h"
static int width(const char*){return 6;}
struct Host {
#ifdef SELECT_ACCENT_VOCAB
 EntryEditor app{width};
 writer::TextModel& text;
 Host():text(app.text()){
  app.open(-1,nullptr);app.frame(0);app.frame(16);app.frame(0);
  assert(app.screen()==EntryEditor::Screen::front);
 }
#else
 std::string root="/tmp/writer-select-accent-"+std::to_string(getpid());
 writer::Storage storage{root.c_str()};writer::Application app{storage,width};
 writer::TextModel& text;
 Host():text(app.text()){
  std::filesystem::create_directory(root);app.boot();
  app.frame(16);app.frame(0);app.frame(16);app.frame(0);
  assert(app.scene()==writer::Scene::EDITOR);
 }
 ~Host(){std::filesystem::remove_all(root);}
#endif
 void frame(unsigned held){app.frame(held);}
 void expect(const char* s){if(std::strcmp(text.data(),s)){std::cerr<<"host expected ["<<s<<"] got ["<<text.data()<<"]\n";std::abort();}}
};
int main(){accent_matrix<Host>();std::cout<<"PASS: complete accent matrix through production host acceptance/cancellation boundary\n";}
