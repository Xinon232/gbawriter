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
 InputState s;Sink out;
 auto frame=[&](unsigned held){s.update(held,consume,&out);};
 frame(bit(Button::START)|bit(Button::A));assert(out.text.empty());assert(out.events.back()==EventKind::SAVE);frame(0);assert(out.text.empty());
 frame(bit(Button::L)|bit(Button::UP)|bit(Button::B));assert(out.text=="n");frame(0);
 frame(bit(Button::R));assert(!s.shift_armed());frame(0);assert(s.shift_armed());
 for(int i=0;i<40;++i)frame(0);
 frame(bit(Button::R));frame(0);assert(s.caps()&&!s.shift_armed());
 frame(bit(Button::R));frame(0);assert(!s.caps()&&!s.shift_armed());
 for(int cycle=0;cycle<4;++cycle){
  frame(bit(Button::R));frame(0);assert(s.shift_armed()&&!s.caps());
  for(int idle=0;idle<1000;++idle)frame(0);
  frame(bit(Button::R));frame(0);assert(s.caps()&&!s.shift_armed());
  frame(bit(Button::R));frame(0);assert(!s.caps()&&!s.shift_armed());
 }
 frame(bit(Button::R));frame(0);frame(bit(Button::DOWN)|bit(Button::R));frame(bit(Button::DOWN));frame(bit(Button::DOWN)|bit(Button::R));assert(out.text=="nG");frame(0);
 frame(bit(Button::UP)|bit(Button::LEFT)|bit(Button::A));assert(out.text=="nG");frame(0); // diagonals never type or space
 frame(bit(Button::R));frame(bit(Button::R)|bit(Button::UP));frame(0);assert(!s.shift_armed());
 std::cout<<"PASS: input whole-frame precedence, isolated releases, untimed R cycles\n";
}
