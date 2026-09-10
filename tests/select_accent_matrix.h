#pragma once
#include <string>
#include <vector>
#include <algorithm>
// Expectations are literal UTF-8, independent of alternate_letter/case_text.
struct AccentRow {unsigned group,button;const char* base;std::vector<const char*> low,upper;};
static const std::vector<AccentRow> accent_rows={
 {1,32,"a",{"á","ä","à","â","ã","å","æ"},{"Á","Ä","À","Â","Ã","Å","Æ"}},
 {1,128,"c",{"ç","č","ć"},{"Ç","Č","Ć"}},
 {65,16,"e",{"é","è","ë","ê"},{"É","È","Ë","Ê"}},
 {8,16,"i",{"í","ï","ì","î"},{"Í","Ï","Ì","Î"}},
 {2,32,"n",{"ñ","ń"},{"Ñ","Ń"}},
 {2,16,"o",{"ó","ö","ô","ò","õ","ø","œ"},{"Ó","Ö","Ô","Ò","Õ","Ø","Œ"}},
 {66,128,"s",{"ß","š","ś"},{"ß","Š","Ś"}},
 {4,16,"u",{"ü","ú","ù","û"},{"Ü","Ú","Ù","Û"}},
 {68,16,"y",{"ý","ÿ"},{"Ý","Ÿ"}},
 {68,128,"z",{"ž","ź","ż"},{"Ž","Ź","Ż"}}
};
template<class H> void accent_matrix(){
 constexpr unsigned SELECT=512,START=256,R=128;
 for(const auto& row:accent_rows)for(int mode=0;mode<3;++mode)for(bool first:{false,true}){
  H h;h.text.set_text("éTAIL");h.text.set_caret(2);
  if(mode){h.frame(R);if(mode==2)for(int i=0;i<48;++i)h.frame(R);h.frame(0);}
  const auto& variants=mode?row.upper:row.low;
  auto expect=[&](const char* letter){h.expect((std::string("é")+letter+"TAIL").c_str());assert(h.text.caret_byte()==2+std::strlen(letter));};
  if(first)h.frame(SELECT);
  h.frame(row.group|row.button|(first?SELECT:0));
  if(!first){std::string base=row.base;if(mode)base[0]-=32;expect(base.c_str());
   // No frame-window cutoff: the exact producing combination stays held.
   for(int i=0;i<1000;++i)h.frame(row.group|row.button);
   h.frame(row.group|row.button|SELECT);
  }
  expect(variants[0]);
  for(unsigned cycle=1;cycle<=2*variants.size();++cycle){
   h.frame(row.group|SELECT);h.frame(row.group|row.button|SELECT);
   expect(variants[cycle%variants.size()]);
  }
  h.frame(0);expect(variants[0]);
  h.frame(1|32);h.frame(0); // Shift consumed, Caps persists.
  h.expect((std::string("é")+variants[0]+(mode==2?"A":"a")+"TAIL").c_str());
 }
 // Every order of releasing Select, group, layer and the producing button.
 for(const auto& row:accent_rows)for(bool first:{false,true}){
  std::vector<unsigned> release={SELECT,row.button,row.group&15};if(row.group&64)release.push_back(64);
  std::sort(release.begin(),release.end());
  do{
   H h;if(first)h.frame(SELECT);h.frame(row.group|row.button|(first?SELECT:0));
   unsigned held=row.group|row.button|SELECT;h.frame(held);h.expect(row.low[0]);
   for(auto bit:release){held&=~bit;h.frame(held);for(int i=0;i<40;++i)h.frame(held);h.expect(row.low[0]);}
  }while(std::next_permutation(release.begin(),release.end()));
 }
 // A released producing button/group/layer cannot lend ownership to Select.
 for(const auto& row:accent_rows){
  for(unsigned lost:{row.button,row.group&15,row.group&64}){
   if(!lost)continue;
   H h;h.frame(row.group|row.button);h.frame((row.group|row.button)&~lost);
   h.frame(((row.group|row.button)&~lost)|SELECT);
   h.expect((std::string(row.base)+".").c_str());h.frame(0);
  }
 }
 // Simultaneous Select + full letter chord retains the Select-first path.
 for(const auto& row:accent_rows)for(int mode=0;mode<3;++mode){
  H h;if(mode){h.frame(R);if(mode==2)for(int i=0;i<48;++i)h.frame(R);h.frame(0);}
  h.frame(row.group|row.button|SELECT);h.frame(0);
  h.expect((mode?row.upper:row.low)[0]);
 }
 // Reholding a released direction/layer without a new letter press is stale.
 for(const auto& row:accent_rows)for(unsigned lost:{row.group&15,row.group&64}){
  if(!lost)continue;
  H h;h.frame(row.group|row.button);h.frame((row.group|row.button)&~lost);
  h.frame(row.group|row.button);h.frame(row.group|row.button|SELECT);
  h.expect((std::string(row.base)+".").c_str());
 }
 // g/v keep their existing two-R replacement and case; neither has accents.
 for(unsigned group:{8u,4u})for(int mode=0;mode<3;++mode){
  H h;if(mode){h.frame(R);if(mode==2)for(int i=0;i<48;++i)h.frame(R);h.frame(0);}
  h.frame(group|R);h.frame(group);h.frame(group|R);
  const char* expected=group==4?(mode?"V":"v"):(mode?"G":"g");h.expect(expected);
  h.frame(group|R|SELECT);h.expect(expected);h.frame(START|SELECT);h.frame(0);h.expect(expected);
 }
 // Special sessions have no time window; release the direction for jj/ww.
 // Every other group's repeated R is literal, including both old locations.
 for(unsigned group:{1u,65u,8u,72u,2u,66u,4u,68u})for(int mode=0;mode<3;++mode){
  const char* thirds=group==1?"c":group==65?"f":group==8?"j":group==72?"m":group==2?"p":group==66?"s":group==4?"w":"z";
  std::string third=thirds;if(mode)third[0]-=32;
  auto arm=[&](H& h){if(mode){h.frame(R);if(mode==2)for(int i=0;i<48;++i)h.frame(R);h.frame(0);}};
  for(bool continuous:{false,true})for(int wait:{0,1,1000}){
   H h;arm(h);h.frame(group|R);h.frame(continuous?group:0);
   for(int i=0;i<wait;++i)h.frame(continuous?group:0);
   h.frame(group|R);
   std::string second=mode==2?third:thirds;
   std::string expected=continuous&&(group==8||group==4)?(group==8?(mode?"G":"g"):(mode?"V":"v")):third+second;
   h.expect(expected.c_str());h.frame(0);h.expect(expected.c_str());
  }
 }
 // Changing layer or pressing a different selector cancels special ownership.
 for(unsigned group:{8u,4u}){
  H layer;layer.frame(group|R);layer.frame(group);layer.frame(group|64);layer.frame(group|64|R);
  layer.expect(group==8?"jm":"wz");
  for(unsigned button:{16u,32u}){
   H h;h.frame(group|R);h.frame(group);h.frame(group|button);h.frame(group);h.frame(group|R);
   h.expect(group==8?(button==16?"jij":"jhj"):(button==16?"wuw":"wtw"));
  }
 }
 // Toggle must retain a converted pre-existing letter through both tails.
 for(bool first:{false,true})for(unsigned tail:{0u,START,SELECT}){
  H h;h.text.set_text("old");if(first)h.frame(SELECT);
  h.frame(1|32|(first?SELECT:0));h.frame(1|32|SELECT);h.expect("oldá");
  h.frame(START|SELECT);h.expect(first?"old":"oldá");
  h.frame(tail|32);for(int i=0;i<40;++i)h.frame(tail|32);
  h.frame(32);for(int i=0;i<40;++i)h.frame(32);h.frame(0);h.expect(first?"old":"oldá");
 }
 // Capacity rejection at the real model boundary cannot target previous text.
 for(int mode=0;mode<3;++mode){
  H h;std::string full(writer::TEXT_CAPACITY,'x');h.text.set_text(full.c_str());
  if(mode){h.frame(R);if(mode==2)for(int i=0;i<48;++i)h.frame(R);h.frame(0);}
  h.frame(1|32);h.frame(1|32|SELECT);h.frame(1|SELECT);h.frame(1|32|SELECT);
  h.frame(START|SELECT);h.frame(0);h.expect(full.c_str());assert(!h.text.dirty());
  h.text.set_text("");h.frame(1|32);h.frame(0);h.expect(mode?"A":"a");
 }
 // Accepted ASCII fits but UTF-8 replacement does not: retain that ASCII.
 {H h;std::string full(writer::TEXT_CAPACITY-1,'x');h.text.set_text(full.c_str());
  h.frame(1|32);h.frame(1|32|SELECT);h.expect((full+"a").c_str());
  h.frame(START|SELECT);h.frame(0);h.expect((full+"a").c_str());}
 // Unsupported ordinary letters are unchanged, not a duplicate or a period.
 const unsigned groups[]={1,65,8,72,2,66,4,68};const char* letters[]={"abc","def","hij","klm","nop","qrs","tuw","xyz"};
 const unsigned buttons[]={32,16,128};
 for(unsigned g=0;g<8;++g)for(unsigned b=0;b<3;++b){char base=letters[g][b];
  if(std::strchr("aceinosuyz",base))continue;
  for(int mode=0;mode<3;++mode)for(bool first:{false,true}){
   H h;if(mode){h.frame(R);if(mode==2)for(int i=0;i<48;++i)h.frame(R);h.frame(0);}
   if(first)h.frame(SELECT);
   h.frame(groups[g]|buttons[b]|(first?SELECT:0));
   h.frame(groups[g]|buttons[b]|SELECT);h.frame(0);
   const char* provisional[]={"1","1","(","(","0","0","-","-"};
   char expected[]={char(mode?base-32:base),0};h.expect(first?provisional[g]:expected);
  }
 }
}
