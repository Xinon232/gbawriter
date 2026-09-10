#include "writer_app.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace writer;
static unsigned key(Button b){return 1u<<unsigned(b);}
// Detection lets the baseline run and fail behavior assertions, not compilation.
template<class T> static auto group(const T& t,int)->decltype(t.active_group()){return t.active_group();}
template<class T> static const char* group(const T&,long){return "";}
static void ignore(void*,InputEvent){}
static void groups(){
 InputState s;
 const Button dirs[]={Button::UP,Button::RIGHT,Button::DOWN,Button::LEFT};
 const char* lower[]={"abc","hij","nop","tuw","def","klm","qrs","xyz"};
 const char* upper[]={"ABC","HIJ","NOP","TUW","DEF","KLM","QRS","XYZ"};
 for(int mode=0;mode<3;++mode){
  for(int layer=0;layer<2;++layer)for(int d=0;d<4;++d){
   unsigned mask=key(dirs[d])|(layer?key(Button::L):0);
   s.update(mask,ignore,nullptr);assert(!strcmp(group(s,0),(mode?upper:lower)[layer*4+d]));
   s.update(mask|key(Button::SELECT),ignore,nullptr);assert(!strcmp(group(s,0),(mode?upper:lower)[layer*4+d]));
   s.update(0,ignore,nullptr);assert(!group(s,0)[0]);
  }
  s.update(key(Button::UP)|key(Button::LEFT),ignore,nullptr);assert(!group(s,0)[0]);s.update(0,ignore,nullptr);
  s.update(key(Button::START)|key(Button::UP),ignore,nullptr);assert(!group(s,0)[0]);s.update(0,ignore,nullptr);
  if(mode==1){s.update(key(Button::R),ignore,nullptr);s.update(0,ignore,nullptr);}
  s.update(key(Button::R),ignore,nullptr);
  if(mode==1)for(int i=0;i<48;++i)s.update(key(Button::R),ignore,nullptr);
  s.update(0,ignore,nullptr);
 }
 std::cout<<"PASS: all normal/L active groups, case, release, diagonals and navigation\n";
}
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
template<class T> static auto bar(const T& t,int)->decltype(t.status_visible()){return t.status_visible();}
template<class T> static bool bar(const T&,long){return true;}
static int width(const char*){return 6;}
static void toggle(){
 std::string root="/tmp/writer-v020-"+std::to_string(getpid());
 std::filesystem::remove_all(root);std::filesystem::create_directory(root);
 Storage storage(root.c_str());Application a(storage,width);a.boot();
 auto tap=[&](Button b){a.frame(key(b));a.frame(0);};
 assert(bar(a,0));tap(Button::A);tap(Button::A);assert(a.scene()==Scene::EDITOR);
 const unsigned start=key(Button::START),select=key(Button::SELECT);
 bool visible=true;
 for(bool dirty:{false,true})for(int order=0;order<3;++order)for(int release=0;release<3;++release){
  a.text().set_text("originaléTAIL");a.text().set_caret(10);if(dirty){a.text().insert("x");}
  std::string original=a.text().data();auto caret=a.text().caret_byte();auto ops=storage.operations();
  if(order==1)a.frame(start);
  if(order==2){a.frame(select);a.frame(select|key(Button::UP)|key(Button::B));}
  a.frame(start|select);visible=!visible;assert(bar(a,0)==visible);
  assert(std::string(a.text().data())==original && a.text().caret_byte()==caret && a.text().dirty()==dirty);
  for(int i=0;i<80;++i)a.frame(start|select|key(Button::A)|key(Button::R));
  if(release==1)a.frame(start|key(Button::B));
  if(release==2)a.frame(select|key(Button::UP)|key(Button::B));
  a.frame(0);assert(bar(a,0)==visible);
  assert(a.scene()==Scene::EDITOR && storage.operations()==ops);
  assert(std::string(a.text().data())==original && a.text().caret_byte()==caret && a.text().dirty()==dirty);
  assert(!a.shift()&&!a.caps());
 }
 // A rejected SELECT insertion must never delete preceding text.
 a.text().set_text(std::string(TEXT_CAPACITY,'x').c_str());a.text().set_caret(5);
 a.frame(select);a.frame(start|select);visible=!visible;a.frame(0);
 assert(a.text().bytes()==TEXT_CAPACITY&&a.text().caret_byte()==5&&!a.text().dirty());
 // Cancelling an alphabetic provisional must retain one-shot Shift.
 a.text().set_text("ok");tap(Button::R);assert(a.shift());
 a.frame(select);a.frame(select|key(Button::UP)|key(Button::B));a.frame(start|select);a.frame(0);
 assert(a.shift()&&!strcmp(a.text().data(),"ok")&&!a.text().dirty());
 // A committed SELECT character is no longer provisional.
 tap(Button::SELECT);std::string committed=a.text().data();
 a.frame(start|select);a.frame(0);assert(std::string(a.text().data())==committed);
 // Rearm when chord buttons release, even with an unrelated direction held.
 bool before=bar(a,0);a.frame(start|select|key(Button::UP));assert(bar(a,0)!=before);
 a.frame(key(Button::UP));a.frame(start|select|key(Button::UP));assert(bar(a,0)==before);
 a.frame(key(Button::UP));a.frame(key(Button::UP)|key(Button::B));a.frame(0);
 assert(std::string(a.text().data())==committed+"A");
 // The chord is editor-only; menu SELECT must still open help.
 Application menu(storage,width);menu.boot();menu.frame(start|select);assert(menu.scene()==Scene::HELP&&bar(menu,0));
 std::filesystem::remove_all(root);
 std::cout<<"PASS: editor-only toggle, both orders/releases, provisional rollback, no leakage or storage\n";
}
static void viewport(){
 std::string root="/tmp/writer-view-"+std::to_string(getpid());std::filesystem::create_directory(root);
 Storage storage(root.c_str());Application a(storage,width);a.boot();
 auto tap=[&](unsigned keys){a.frame(keys);a.frame(0);};tap(key(Button::A));tap(key(Button::A));
 std::string lines;for(int i=0;i<40;++i)lines+="line\n";
 a.text().set_text(lines.c_str());a.layout().reflow(a.text(),220,width);a.text().move_home();
 tap(key(Button::START)|key(Button::R));assert(a.layout().position(a.text(),a.text().caret_byte()).row==7);
 tap(key(Button::START)|key(Button::SELECT));assert(!bar(a,0));
 tap(key(Button::START)|key(Button::R));assert(a.layout().position(a.text(),a.text().caret_byte()).row==16);
 tap(key(Button::START)|key(Button::L));assert(a.layout().position(a.text(),a.text().caret_byte()).row==7);
 a.text().move_end();tap(key(Button::START)|key(Button::SELECT));
 assert(a.viewport()==34); // row 40 is the seventh visible row
 tap(key(Button::START)|key(Button::SELECT));
 assert(a.viewport()<=40 && 40<a.viewport()+9);
 tap(key(Button::START)|key(Button::A));assert(!bar(a,0));
 storage.fault_at(1);tap(key(Button::START)|key(Button::A));assert(a.scene()==Scene::ERROR);
 tap(key(Button::A));assert(a.scene()==Scene::EDITOR&&!bar(a,0));storage.fault_at(-1);
 std::filesystem::remove_all(root);std::cout<<"PASS: dynamic 7/9-row pages and visibility, save/error mode preservation\n";
}
static void dates(){
 std::string root="/tmp/writer-date-"+std::to_string(getpid());std::filesystem::create_directory(root);
 Storage storage(root.c_str());Application a(storage,width);a.boot();
 auto tap=[&](Button b){a.frame(key(b));a.frame(0);};tap(Button::A);
 tap(Button::LEFT);assert(a.date().day==9&&a.date_field()==0);
 tap(Button::RIGHT);assert(a.date().day==10);
 tap(Button::UP);assert(a.date_field()==2);tap(Button::RIGHT);assert(a.date().year==2027);
 tap(Button::DOWN);assert(a.date_field()==0);tap(Button::DOWN);assert(a.date_field()==1);
 tap(Button::LEFT);assert(a.date().month==6);tap(Button::RIGHT);assert(a.date().month==7);
 std::filesystem::remove_all(root);std::cout<<"PASS: date horizontal values and vertical fields\n";
}
template<class T> static auto saving(const T& t,unsigned mask,int)->decltype(t.save_feedback(mask)){return t.save_feedback(mask);}
template<class T> static bool saving(const T&,unsigned,long){return true;}
static void indicators(){
 std::string root="/tmp/writer-indicator-"+std::to_string(getpid());std::filesystem::create_directory(root);
 Storage storage(root.c_str());Application a(storage,width);a.boot();
 auto tap=[&](unsigned mask){a.frame(mask);a.frame(0);};tap(key(Button::A));tap(key(Button::A));
 a.frame(key(Button::UP));assert(!strcmp(group(a,0),"abc"));a.take_redraw();a.frame(0);assert(a.take_redraw());
 unsigned start=key(Button::START),select=key(Button::SELECT),save=start|key(Button::A);
 assert(saving(a,save,0));assert(!saving(a,save|select,0));
 a.frame(start|select);assert(!saving(a,save,0));a.frame(start);assert(!saving(a,save,0));a.frame(0);assert(saving(a,save,0));
 std::filesystem::remove_all(root);std::cout<<"PASS: group release redraw and toggle-safe pre-save feedback\n";
}
static void select_release_save_feedback(){
 for(Button command:{Button::A,Button::B}){
  std::string root="/tmp/writer-release-save-"+std::to_string(getpid());
  std::filesystem::create_directory(root);
  Storage storage(root.c_str());Application a(storage,width);a.boot();
  auto tap=[&](unsigned mask){a.frame(mask);a.frame(0);};
  tap(key(Button::A));tap(key(Button::A));assert(a.scene()==Scene::EDITOR);
  a.frame(key(Button::SELECT));assert(!strcmp(a.text().data(),"."));
  unsigned save=key(Button::START)|key(command);
  assert(a.save_feedback(save) && "SELECT release must not hide fresh save feedback");
  auto operations=storage.operations();
  a.frame(save);a.frame(0);
  assert(storage.operations()>operations && !a.text().dirty());
  assert(a.scene()==(command==Button::A?Scene::EDITOR:Scene::MENU));
  assert(!strcmp(a.text().data(),"."));
  std::ifstream file(root+"/gbawriter/"+storage.current_name());std::string bytes((std::istreambuf_iterator<char>(file)),{});
  assert(bytes==".");
  std::filesystem::remove_all(root);
 }
 std::cout<<"PASS: SELECT release plus fresh START+A/B shows feedback and saves committed text\n";
}
int main(){groups();toggle();viewport();dates();indicators();select_release_save_feedback();}
