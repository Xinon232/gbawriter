#include "writer_core.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace writer;
constexpr unsigned R=1u<<unsigned(Button::R);
static void ignore(void*,InputEvent){}
int main(){
  // Initial press is elapsed frame zero, matching solo-A repeat.
  InputState s;
  s.update(R,ignore,nullptr);
  for(int elapsed=1;elapsed<48;++elapsed){
    s.update(R,ignore,nullptr);
    assert(!s.caps()&&!s.shift_armed());
  }
  s.update(R,ignore,nullptr);
  assert(s.caps()&&!s.shift_armed());
  for(int i=0;i<200;++i)s.update(R,ignore,nullptr);
  assert(s.caps()&&!s.shift_armed());
  s.update(0,ignore,nullptr);
  assert(s.caps()&&!s.shift_armed());
  // Active modes clear only on isolated release, even for a very long hold.
  for(bool caps:{false,true})for(int duration:{1,47,48,49,400}){
    InputState mode;
    mode.update(R,ignore,nullptr);
    if(caps)for(int i=0;i<48;++i)mode.update(R,ignore,nullptr);
    mode.update(0,ignore,nullptr);
    assert(mode.caps()==caps && mode.shift_armed()!=caps);
    for(int i=0;i<duration;++i){
      mode.update(R,ignore,nullptr);
      assert(mode.caps()==caps && mode.shift_armed()!=caps);
    }
    mode.update(0,ignore,nullptr);
    assert(!mode.caps()&&!mode.shift_armed());
    mode.update(R,ignore,nullptr);
    for(int i=0;i<48;++i)mode.update(R,ignore,nullptr);
    assert(mode.caps()&&!mode.shift_armed());
  }
  // Any companion cancels eligibility for the entire R session, in either
  // press/release order, including a new companion on the threshold frame.
  for(unsigned other=0;other<10;++other){
    unsigned companion=1u<<other;
    if(companion==R)continue;
    for(int arrival:{0,1,47,48})for(bool other_first:{false,true})
    for(bool release_r_first:{false,true}){
      InputState chord;
      if(other_first)chord.update(companion,ignore,nullptr);
      if(arrival){
        chord.update(R|(other_first?companion:0),ignore,nullptr);
        for(int i=1;i<arrival;++i)chord.update(R|(other_first?companion:0),ignore,nullptr);
      }
      chord.update(R|companion,ignore,nullptr);
      for(int i=0;i<80;++i)chord.update(R|companion,ignore,nullptr);
      assert(!chord.caps()&&!chord.shift_armed());
      unsigned tail=release_r_first?companion:R;
      for(int i=0;i<100;++i)chord.update(tail,ignore,nullptr);
      chord.update(0,ignore,nullptr);
      assert(!chord.caps()&&!chord.shift_armed());
      chord.update(R,ignore,nullptr);chord.update(0,ignore,nullptr);
      assert(chord.shift_armed()&&!chord.caps());
    }
  }
  // Per-button callers get the same isolated release disambiguation.
  InputState direct;
  direct.press(Button::R,true);
  assert(!direct.shift_armed());
  direct.release(Button::R);
  assert(direct.shift_armed());
  direct.press(Button::R,true);
  assert(direct.shift_armed());
  direct.release(Button::R);
  assert(!direct.shift_armed()&&!direct.caps());
  direct.press(Button::R,true);direct.press(Button::A,true);
  direct.release(Button::A);direct.release(Button::R);
  assert(!direct.shift_armed()&&!direct.caps());
  // Public events and whole-frame polling share one continuous R session.
  for(unsigned other=0;other<10;++other){
    if((1u<<other)==R)continue;
    InputState mixed;
    mixed.press(Button::R,true);
    mixed.press(static_cast<Button>(other),true);
    mixed.release(static_cast<Button>(other));
    for(int i=0;i<100;++i)mixed.update(R,ignore,nullptr);
    assert(!mixed.caps()&&!mixed.shift_armed());
    mixed.update(0,ignore,nullptr);
    assert(!mixed.caps()&&!mixed.shift_armed());
    mixed.press(Button::R,true);
    mixed.update(0,ignore,nullptr);
    assert(mixed.shift_armed()&&!mixed.caps());
  }
  for(bool event_press:{false,true})for(bool event_release:{false,true})
  for(int elapsed:{0,1,47,48,49,100}){
    InputState mixed;
    if(event_press)mixed.press(Button::R,true);
    else mixed.update(R,ignore,nullptr);
    for(int i=0;i<elapsed;++i){
      mixed.update(R,ignore,nullptr);
      assert(mixed.caps()==(i+1>=48));
    }
    assert(mixed.caps()==(elapsed>=48));
    if(event_release)mixed.release(Button::R);
    else mixed.update(0,ignore,nullptr);
    for(int i=0;i<100;++i)mixed.update(0,ignore,nullptr);
    assert(mixed.caps()==(elapsed>=48));
    assert(mixed.shift_armed()==(elapsed<48));
    // The next isolated hold clears on release, never rearming while held.
    mixed.press(Button::R,true);
    for(int i=0;i<100;++i)mixed.update(R,ignore,nullptr);
    assert(mixed.caps()==(elapsed>=48));
    assert(mixed.shift_armed()==(elapsed<48));
    mixed.release(Button::R);
    for(int i=0;i<100;++i)mixed.update(0,ignore,nullptr);
    assert(!mixed.caps()&&!mixed.shift_armed());
  }
  static_assert(InputState::CAPS_HOLD_DELAY == 2*InputState::NAV_REPEAT_DELAY);
  for(int elapsed:{0,1,47,48,49,400}){
    InputState boundary;
    boundary.update(R,ignore,nullptr);
    for(int i=0;i<elapsed;++i)boundary.update(R,ignore,nullptr);
    assert(boundary.caps()==(elapsed>=48));
    boundary.update(0,ignore,nullptr);
    assert(boundary.caps()==(elapsed>=48));
    assert(boundary.shift_armed()==(elapsed<48));
  }
  // A canceled active-mode clearing attempt is still a chord, not a clear.
  // Directions alone do not insert a letter or consume one-shot Shift.
  for(bool caps:{false,true}){
    InputState mode;mode.update(R,ignore,nullptr);
    if(caps)for(int i=0;i<48;++i)mode.update(R,ignore,nullptr);
    mode.update(0,ignore,nullptr);
    mode.update(R,ignore,nullptr);mode.update(R|1,ignore,nullptr);
    for(int i=0;i<100;++i)mode.update(R,ignore,nullptr);
    mode.update(0,ignore,nullptr);
    assert(mode.caps()==caps&&mode.shift_armed()!=caps);
    mode.update(R,ignore,nullptr);mode.reset_transient();
    mode.update(0,ignore,nullptr);
    assert(mode.caps()==caps&&mode.shift_armed()!=caps);
  }
  InputState transition;transition.update(R,ignore,nullptr);
  transition.reset_transient();transition.update(0,ignore,nullptr);
  assert(!transition.caps()&&!transition.shift_armed());
  std::puts("PASS: solo R boundaries, clearing, chords, public releases and transient reset");
}
