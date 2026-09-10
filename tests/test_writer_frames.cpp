#include "writer_core.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
using namespace writer;
static unsigned bit(Button b){return 1u<<unsigned(b);}
struct Sink {std::string text;std::vector<EventKind> events;};
static void consume(void* context,InputEvent e){auto& s=*static_cast<Sink*>(context);s.events.push_back(e.kind);if(e.kind==EventKind::INSERT)s.text+=e.text;if(e.kind==EventKind::REPLACE){assert(!s.text.empty());s.text.back()=e.text[0];}}
int main(){
 // Each isolated edit has the exact navigation repeat schedule, including wrap.
 for(Button button:{Button::A,Button::B}){
  InputState edit,nav;Sink edits,moves;
  for(int frame=0;frame<200;++frame){
   edit.update(bit(button),consume,&edits);
   nav.update(bit(Button::START)|bit(Button::LEFT),consume,&moves);
   const bool due=frame==0 || (frame>=24 && (frame-24)%5==0);
   assert(edits.events.size()==moves.events.size());
   if(due)assert(edits.events.back()==(button==Button::A?EventKind::INSERT:EventKind::BACKSPACE));
   assert(edits.events.size()==std::size_t(1+(frame<24?0:1+(frame-24)/5)));
  }
  auto count=edits.events.size();edit.update(0,consume,&edits);
  for(int i=0;i<60;++i)edit.update(0,consume,&edits);
  assert(edits.events.size()==count);
  edit.update(bit(button),consume,&edits);assert(edits.events.size()==count+1);
  for(int i=1;i<24;++i)edit.update(bit(button),consume,&edits);
  assert(edits.events.size()==count+1);
  edit.update(bit(button),consume,&edits);assert(edits.events.size()==count+2);
 }
 // Switching directly to the other solo button starts a fresh delay.
 for(Button first:{Button::A,Button::B}){
  InputState input;Sink sink;Button second=first==Button::A?Button::B:Button::A;
  for(int i=0;i<80;++i)input.update(bit(first),consume,&sink);
  auto count=sink.events.size();input.update(bit(second),consume,&sink);
  assert(sink.events.size()==count+1);
  for(int i=1;i<24;++i)input.update(bit(second),consume,&sink);
  assert(sink.events.size()==count+1);
  input.update(bit(second),consume,&sink);assert(sink.events.size()==count+2);
 }
 // Every additional button cancels solo repeat; chord tails cannot rearm it.
 for(Button button:{Button::A,Button::B})for(unsigned other=0;other<10;++other){
  if(bit(button)==(1u<<other))continue;
  for(bool solo_first:{false,true}){
   InputState input;Sink sink;
   auto tick=[&](unsigned keys){input.update(keys,consume,&sink);};
   if(solo_first){tick(bit(button));for(int i=0;i<30;++i)tick(bit(button));}
   const unsigned chord=bit(button)|(1u<<other);
   tick(chord);auto count=sink.events.size();
   for(int i=0;i<100;++i)tick(chord);
   assert(sink.events.size()==count); // existing initial chord event only
   tick(bit(button));count=sink.events.size(); // preserve modifier release semantics
   for(int i=0;i<100;++i)tick(bit(button));
   assert(sink.events.size()==count);
   tick(0);count=sink.events.size();tick(bit(button));
   assert(sink.events.size()==count+1);
   for(int i=1;i<=24;++i)tick(bit(button));
   assert(sink.events.size()==count+2);
  }
  InputState input;Sink sink;
  input.update(bit(Button::START)|bit(Button::SELECT)|bit(button),consume,&sink);
  input.update(bit(button),consume,&sink);auto count=sink.events.size();
  for(int i=0;i<100;++i)input.update(bit(button),consume,&sink);
  assert(sink.events.size()==count); // toggle-latched release tail
  input.update(0,consume,&sink);input.update(bit(button),consume,&sink);
  for(int i=1;i<=24;++i)input.update(bit(button),consume,&sink);
  assert(sink.events.size()==count+2);
 }
 InputState s;Sink out;
 auto frame=[&](unsigned held){s.update(held,consume,&out);};
 frame(bit(Button::START)|bit(Button::A));assert(out.text.empty());assert(out.events.back()==EventKind::SAVE);frame(0);assert(out.text.empty());
 frame(bit(Button::L)|bit(Button::UP)|bit(Button::B));assert(out.text=="d");frame(0);
 frame(bit(Button::R));assert(!s.shift_armed());frame(0);assert(s.shift_armed());
 for(int i=0;i<40;++i)frame(0);
 frame(bit(Button::R));frame(0);assert(!s.caps()&&!s.shift_armed());
  for(int i=0;i<=48;++i){frame(bit(Button::R));}frame(0);assert(s.caps()&&!s.shift_armed());
 frame(bit(Button::R));frame(0);assert(!s.caps()&&!s.shift_armed());
 for(int cycle=0;cycle<4;++cycle){
  frame(bit(Button::R));frame(0);assert(s.shift_armed()&&!s.caps());
  for(int idle=0;idle<1000;++idle)frame(0);
  frame(bit(Button::R));frame(0);assert(!s.caps()&&!s.shift_armed());
  for(int i=0;i<=48;++i){frame(bit(Button::R));}frame(0);assert(s.caps()&&!s.shift_armed());
  frame(bit(Button::R));frame(0);assert(!s.caps()&&!s.shift_armed());
 }
 frame(bit(Button::R));frame(0);frame(bit(Button::RIGHT)|bit(Button::R));frame(bit(Button::RIGHT));frame(bit(Button::RIGHT)|bit(Button::R));assert(out.text=="dG");frame(0);
 frame(bit(Button::UP)|bit(Button::LEFT)|bit(Button::A));assert(out.text=="dG");frame(0); // diagonals never type or space
 frame(bit(Button::R));frame(bit(Button::R)|bit(Button::UP));frame(0);assert(!s.shift_armed());
 std::cout<<"PASS: input whole-frame precedence, isolated releases, isolated R short/hold sessions\n";
}
