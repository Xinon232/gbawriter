#include "writer_core.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace writer;
constexpr unsigned key(Button b){return 1u<<unsigned(b);}
constexpr unsigned U=key(Button::UP),B=key(Button::B),S=key(Button::SELECT);
// Mirror the exact provisional acceptance boundary shared by both hosts.
struct Harness {
 InputState input; TextModel text; bool provisional=false,dirty=false; unsigned end=0;
 static void consume(void* p,InputEvent e){
  auto& h=*static_cast<Harness*>(p); bool ok=true;
  switch(e.kind){
  case EventKind::INSERT:
   if(h.input.select_active())h.dirty=h.text.dirty();
   ok=h.text.insert(e.text);
   if(h.input.select_active()){h.provisional=ok;h.end=h.text.caret_byte();}break;
  case EventKind::REPLACE:
   if(h.input.select_active()&&(!h.provisional||h.text.caret_byte()!=h.end))ok=false;
   else ok=h.text.replace_before_caret(e.text);
   if(h.input.select_active()&&ok)h.end=h.text.caret_byte();
   break;
  case EventKind::TOGGLE_STATUS:
   if(h.input.select_active()&&h.provisional&&h.text.caret_byte()==h.end){h.text.backspace();if(!h.dirty)h.text.mark_saved();}
   h.provisional=false;break;
  case EventKind::BACKSPACE:h.text.backspace();break;
  default:break;
  }
  if(!ok)h.input.reject_edit();
 }
 void frame(unsigned held){input.update(held,consume,this);}
 void expect(const char* s){if(std::strcmp(text.data(),s)){std::cerr<<"expected ["<<s<<"] got ["<<text.data()<<"]\n";std::abort();}}
};
#include "select_accent_matrix.h"
int main(){
 accent_matrix<Harness>();
 Harness h;h.frame(U|B);h.expect("a");h.frame(U|B|S);h.expect("á");h.frame(0);h.expect("á");
 constexpr unsigned D=key(Button::DOWN),R=key(Button::R),L=key(Button::L);
 for(unsigned layer:{0u,L}){
  Harness special;special.frame(layer|D|R);special.frame(layer|D);special.frame(layer|D|R);
  special.expect(layer?"v":"g");special.frame(layer|D|R|S);special.expect(layer?"v":"g");
 }
 Harness ambiguous;ambiguous.frame(U|B|key(Button::A));ambiguous.expect("ab");
 ambiguous.frame(U|B|key(Button::A)|S);ambiguous.expect("ab.");
 // Public per-button API also invalidates a group that changed and returned.
 InputState direct;direct.press(Button::UP,true);direct.press(Button::B,true);
 direct.press(Button::L,true);direct.release(Button::L);
 assert(direct.press(Button::SELECT,true).kind==EventKind::INSERT);
 InputState rejected;rejected.press(Button::UP,true);rejected.press(Button::B,true);rejected.reject_edit();
 assert(rejected.press(Button::SELECT,true).kind==EventKind::INSERT);
 std::cout<<"PASS: continuous held letter replacement, g/v, exact producing button eligibility\n";
}
