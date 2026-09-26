#include "writer_storage.h"
#include "diskio.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
using namespace writer;
static FILE* image; static FATFS fs; static int checks=0, failures=0;
static long reads=0,writes=0,syncs=0,events=0,fail_at=-1; static bool fired=false;
static bool fault(const char* kind,long sector=-1){++events;if(events==fail_at){fired=true;printf("INJECT event=%ld kind=%s sector=%ld\n",events,kind,sector);return true;}return false;}
extern "C" {
DSTATUS disk_initialize(BYTE d){return d?STA_NOINIT:0;}
DSTATUS disk_status(BYTE d){return d?STA_NOINIT:0;}
DRESULT disk_read(BYTE d,BYTE* b,LBA_t s,UINT n){++reads;if(d||fault("read",s))return RES_ERROR;return fseek(image,(long)s*512,SEEK_SET)||fread(b,512,n,image)!=n?RES_ERROR:RES_OK;}
DRESULT disk_write(BYTE d,const BYTE* b,LBA_t s,UINT n){++writes;if(d||fault("write",s))return RES_ERROR;return fseek(image,(long)s*512,SEEK_SET)||fwrite(b,512,n,image)!=n?RES_ERROR:RES_OK;}
DRESULT disk_ioctl(BYTE d,BYTE cmd,void* b){if(d)return RES_PARERR;if(cmd==CTRL_SYNC){++syncs;if(fault("sync"))return RES_ERROR;return fflush(image)||fsync(fileno(image))?RES_ERROR:RES_OK;}if(cmd==GET_SECTOR_COUNT){*(LBA_t*)b=32768;return RES_OK;}if(cmd==GET_SECTOR_SIZE){*(WORD*)b=512;return RES_OK;}if(cmd==GET_BLOCK_SIZE){*(DWORD*)b=1;return RES_OK;}return RES_PARERR;}
}
static void check(bool ok,const std::string& s){++checks;if(!ok){++failures;printf("FAIL %s\n",s.c_str());}}
static void must(bool ok,const char* s){if(!ok){printf("FIXTURE ERROR %s\n",s);exit(2);}}
static std::string path(const std::string& n){return "/gbawriter/"+n;}
static void put(const std::string& n,const std::string& data){FIL f;UINT w=0;must(f_open(&f,path(n).c_str(),FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"put open");must(f_write(&f,data.data(),data.size(),&w)==FR_OK&&w==data.size(),"put write");must(f_close(&f)==FR_OK,"put close");}
static bool exists(const std::string& n){FILINFO i;return f_stat(path(n).c_str(),&i)==FR_OK;}
static std::string get(const std::string& n){FIL f;must(f_open(&f,path(n).c_str(),FA_READ)==FR_OK,"get open");std::string s(f_size(&f),'\0');UINT r;FRESULT result=f_read(&f,s.data(),s.size(),&r);if(result!=FR_OK||r!=s.size()){printf("READ_ERROR name=%s result=%d read=%u expected=%zu\n",n.c_str(),int(result),r,s.size());f_close(&f);return "<UNREADABLE>";}must(f_close(&f)==FR_OK,"get close");return s;}
static void dump(const std::string& n){fflush(image);std::vector<unsigned char> b(32768*512);rewind(image);must(fread(b.data(),1,b.size(),image)==b.size(),"dump read");FILE* o=fopen(n.c_str(),"wb");must(o&&fwrite(b.data(),1,b.size(),o)==b.size(),"dump write");fclose(o);}
static std::vector<unsigned char> image_bytes(){must(!fflush(image),"image flush");std::vector<unsigned char> b(32768*512);rewind(image);must(fread(b.data(),1,b.size(),image)==b.size(),"image read");return b;}
static void remount(){must(f_mount(nullptr,"0:",0)==FR_OK,"unmount");std::memset(&fs,0,sizeof fs);must(f_mount(&fs,"0:",1)==FR_OK,"mount");}
static bool clean(const std::string& n){return !exists(n+".gwt")&&!exists(n+".gwb")&&!exists(n+".gwi");}
int main(int argc,char**argv){must(argc==2,"image argument");image=fopen(argv[1],"r+b");must(image,"image open");remount();must(fs.fs_type==FS_FAT16,"actual FAT16");must(f_mkdir("/gbawriter")==FR_OK,"mkdir");Storage s;TextModel t;
check(s.scan()==StoreResult::OK&&s.count()==0,"empty scan");
check(s.create("Mixed Name.TXT",t)==StoreResult::OK,"LFN uppercase extension create");
std::string utf8=u8"é Ελληνικά 日本語 😀\r\nline two\n";check(t.insert(utf8.c_str()),"insert UTF8");check(s.save(t)==StoreResult::OK&&!t.dirty(),"first save marks clean");check(get("Mixed Name.TXT")==utf8&&clean("Mixed Name.TXT"),"exact UTF8 no footer or sidecars");
check(s.create("mixed name.txt",t)==StoreResult::EXISTS&&get("Mixed Name.TXT")==utf8,"case insensitive exclusive create preserves content");
check(s.load("MIXED NAME.txt",t)==StoreResult::OK&&std::string(t.str().c_str())==utf8,"case insensitive load exact");
std::string large(TEXT_CAPACITY,'a');large.replace(510,4,u8"😀");check(t.set_text(large.c_str()),"capacity UTF8");check(t.insert("x")==false,"over capacity edit refused");check(s.save(t)==StoreResult::OK&&get("Mixed Name.TXT")==large,"24KiB save including split UTF8 sector");
check(t.set_text("short\n")&&s.save(t)==StoreResult::OK&&get("Mixed Name.TXT")=="short\n","sequential shorter replacement no tail");check(t.set_text("")&&s.save(t)==StoreResult::OK&&get("Mixed Name.TXT").empty(),"sequential empty replacement");
remount();check(s.load("Mixed Name.TXT",t)==StoreResult::OK&&t.bytes()==0,"remount persisted");
for(const char* n:{"../evil.txt","a/b.txt","a\\b.txt","0:x.txt","bad.bin",""})check(s.create(n,t)==StoreResult::INVALID_NAME,std::string("reject path ")+n);
put("bad.txt",std::string("a\0b",3));put("over.txt",std::string(TEXT_CAPACITY+1,'x'));put("invalid.txt",std::string("\xc0\xaf",2));t.set_text("keep");t.insert("!");std::string kept=t.str().c_str();check(s.load("bad.txt",t)==StoreResult::INVALID_UTF8&&std::string(t.str().c_str())==kept&&t.dirty(),"NUL refused without buffer loss");check(s.load("invalid.txt",t)==StoreResult::INVALID_UTF8&&std::string(t.str().c_str())==kept,"invalid UTF8 refused");check(s.load("over.txt",t)==StoreResult::OK&&t.str()==std::string(TEXT_CAPACITY+1,'x')&&!t.dirty()&&t.unsaved_bytes()==0,"file larger than the typed-text buffer streams from SD");
put("09072026.txt","");put("11072026.TXT","");put("zebra.txt","");put("Alpha.txt","");put("ignored.bin","");must(f_mkdir("/gbawriter/folder.txt")==FR_OK,"directory fixture");check(s.scan()==StoreResult::OK,"sorted scan");for(int i=0;i<s.count();++i)printf("SCAN %s\n",s.name(i));check(std::string(s.name(0))=="11072026.TXT"&&std::string(s.name(1))=="09072026.txt"&&std::string(s.name(2))=="Alpha.txt","dates newest first then case insensitive alpha");check(s.proposed_date().day==12&&s.proposed_date().month==7&&s.proposed_date().year==2026,"next diary date");check(s.total()==8,"ignore non txt and directories");
put("recover.txt.gwb","old");put("recover.txt.gwt","new");check(s.recover("recover.txt")==StoreResult::OK&&get("recover.txt")=="old"&&clean("recover.txt"),"restore missing canonical from backup");
put("ambig.txt","new");put("ambig.txt.gwb","old");check(s.recover("ambig.txt")==StoreResult::RECOVERY_NEEDED&&get("ambig.txt")=="new"&&get("ambig.txt.gwb")=="old","ambiguous copies retained");
put("stage.txt","old");put("stage.txt.gwt","partial");check(s.load("stage.txt",t)==StoreResult::OK,"stage load");t.insert("edit");check(s.save(t)==StoreResult::RECOVERY_NEEDED&&t.dirty()&&get("stage.txt")=="old","existing staging blocks save and retains dirty");auto stage_before=image_bytes();long stage_writes=writes;check(s.recover("stage.txt")==StoreResult::RECOVERY_NEEDED&&get("stage.txt")=="old"&&get("stage.txt.gwt")=="partial"&&writes==stage_writes&&image_bytes()==stage_before,"staging recovery preserves both copies without disk mutation");
// Snapshot a clean transaction target. Each run restores exact on-disk baseline.
put("fault.txt","original bytes\n");remount();fflush(image);std::vector<unsigned char> baseline(32768*512);rewind(image);must(fread(baseline.data(),1,baseline.size(),image)==baseline.size(),"snapshot");
std::string replacement(1700,'R');replacement+=utf8;long boundary=0;int injected=0,reported=0,recovery_needed=0;
for(long at=0;;++at){fail_at=-1;f_mount(nullptr,"0:",0);rewind(image);must(fwrite(baseline.data(),1,baseline.size(),image)==baseline.size()&&!fflush(image),"restore image");remount();Storage f;TextModel edit;must(f.load("fault.txt",edit)==StoreResult::OK,"fault fixture load");must(edit.set_text(replacement.c_str())&&edit.insert("!"),"fault edit");std::string expected=edit.str().c_str();events=0;fired=false;fail_at=at?at:-1;StoreResult r=f.save(edit);long used=events;fail_at=-1;if(!at){boundary=used;printf("SAVE_DISK_BOUNDARIES %ld\n",boundary);}else{check(fired,"injection fired "+std::to_string(at));++injected;if(r!=StoreResult::OK)++reported;check(r==StoreResult::OK||edit.dirty(),"failure keeps dirty "+std::to_string(at));}
if(at>=99&&at<=101)dump("fault"+std::to_string(at)+"-before-recovery.img");int before=failures;remount();auto recovery_before=image_bytes();long recovery_writes=writes;StoreResult rr=f.recover("fault.txt");if(rr==StoreResult::RECOVERY_NEEDED){++recovery_needed;check(writes==recovery_writes&&image_bytes()==recovery_before,"manual recovery preserves whole image "+std::to_string(at));}if(at>=99&&at<=101){check(rr==StoreResult::RECOVERY_NEEDED,"crosslink refused "+std::to_string(at));check(get("fault.txt.gwt")==expected&&get("fault.txt.gwb")=="original bytes\n"&&exists("fault.txt.gwi"),"crosslink retains exact new, old and manifest "+std::to_string(at));dump("fault"+std::to_string(at)+"-after-recovery.img");}remount();check(rr==StoreResult::OK||rr==StoreResult::RECOVERY_NEEDED,"recover result "+std::to_string(at));check(exists("fault.txt"),"canonical exists "+std::to_string(at));if(exists("fault.txt")){auto value=get("fault.txt");check(value=="original bytes\n"||value==expected,"canonical old or exact new "+std::to_string(at));if(r==StoreResult::OK)check(value==expected,"reported saved persisted "+std::to_string(at));}if(failures>before)dump("fault"+std::to_string(at)+"-after-recovery.img");printf("FAULT %ld save=%d recover=%d fired=%d\n",at,int(r),int(rr),int(fired));if(at==boundary)break;}
printf("FAULT_SUMMARY injected=%d save_non_ok=%d manual_recovery=%d\n",injected,reported,recovery_needed);
// A document that still reads its file from SD (larger than the typed-text
// buffer), edited in the middle, saved with a fault at every disk event. The
// session text must stay exact and readable, a same-session retry must never
// delete the file the session reads, and the canonical file is old or new.
{
std::string big;for(int i=0;big.size()<100000;++i){big+="line "+std::to_string(i)+u8" é 日本 😀 ";if(i%7==0)big+="\n";}
put("big.txt",big);remount();auto big_base=image_bytes();long big_boundary=0;int big_failed=0,big_retried=0,big_refused=0;
for(long at=0;;++at){fail_at=-1;f_mount(nullptr,"0:",0);rewind(image);must(fwrite(big_base.data(),1,big_base.size(),image)==big_base.size()&&!fflush(image),"restore big image");remount();
 Storage f;TextModel edit;must(f.load("big.txt",edit)==StoreResult::OK&&edit.bytes()==big.size()&&edit.unsaved_bytes()==0,"big load streams");
 edit.set_caret(big.size()/2);must(edit.insert("INSERTED\n"),"big insert");edit.set_caret(1000);must(edit.backspace()&&edit.backspace(),"big erase");edit.move_end();must(edit.insert("END"),"big append");
 std::string expected=edit.str();must(!edit.read_failed()&&expected.size()>big.size()&&expected.find("INSERTED\n")!=std::string::npos,"big expected");
 events=0;fired=false;fail_at=at?at:-1;StoreResult r=f.save(edit);long used=events;fail_at=-1;
 if(!at){big_boundary=used;printf("BIG_SAVE_DISK_BOUNDARIES %ld\n",big_boundary);check(r==StoreResult::OK,"big clean save");}
 else check(fired,"big injection fired "+std::to_string(at));
 edit.clear_read_failed();std::string session=edit.str();
 check(!edit.read_failed()&&session==expected,"big session text exact after save "+std::to_string(at));
 if(r==StoreResult::OK)check(!edit.dirty()&&edit.unsaved_bytes()==0&&get("big.txt")==expected,"big saved, reopened, buffer free "+std::to_string(at));
 else{++big_failed;check(edit.dirty(),"big failure keeps dirty "+std::to_string(at));
  // Same session, as the app does before a save: recover, then save again.
  StoreResult rr=f.recover("big.txt");check(rr==StoreResult::OK||rr==StoreResult::RECOVERY_NEEDED,"big recover result "+std::to_string(at));
  edit.clear_read_failed();check(edit.str()==expected&&!edit.read_failed(),"big recovery keeps session source "+std::to_string(at));
  if(rr==StoreResult::OK){++big_retried;StoreResult r2=f.save(edit);check(r2==StoreResult::OK&&get("big.txt")==expected&&!edit.dirty(),"big retry saves "+std::to_string(at));}
  else ++big_refused;}
 remount();Storage g;StoreResult gr=g.recover("big.txt");check(gr==StoreResult::OK||gr==StoreResult::RECOVERY_NEEDED,"big fresh recover "+std::to_string(at));
 check(exists("big.txt"),"big canonical exists "+std::to_string(at));
 if(exists("big.txt")){auto value=get("big.txt");check(value==big||value==expected,"big canonical old or exact new "+std::to_string(at));}
 if(at==big_boundary)break;}
printf("BIG_FAULT_SUMMARY failed=%d retried=%d refused=%d\n",big_failed,big_retried,big_refused);
must(f_unlink("/gbawriter/big.txt")==FR_OK,"remove big fixture");
for(const char* n:{"big.txt.gwt","big.txt.gwb","big.txt.gwi"})if(exists(n))must(f_unlink(("/gbawriter/"+std::string(n)).c_str())==FR_OK,"remove big staging");
}
must(f_unlink("/gbawriter/ambig.txt.gwb")==FR_OK,"resolve intentional ambiguity before scan");
auto scan_before=image_bytes();long scan_writes=writes;check(s.scan()==StoreResult::RECOVERY_NEEDED&&get("stage.txt")=="old"&&get("stage.txt.gwt")=="partial"&&writes==scan_writes&&image_bytes()==scan_before,"scan refuses staging ambiguity without disk mutation");
// Explicit test-fixture resolution: rename the independently created staging copy,
// preserving its bytes outside the reserved artifact namespace; never unlink it.
must(f_rename("/gbawriter/stage.txt.gwt","/gbawriter/preserved-stage.bin")==FR_OK,"preserve staging fixture under non-artifact name");
check(get("preserved-stage.bin")=="partial"&&get("stage.txt")=="old","manual fixture resolution preserves both copies");
for(int i=0;i<40;++i){char n[32];snprintf(n,sizeof n,"page%02d.txt",i);put(n,"");}StoreResult page_result=s.scan();printf("PAGE result=%d count=%d total=%d\n",int(page_result),s.count(),s.total());check(page_result==StoreResult::OK&&s.count()==FILE_PAGE_SIZE&&s.total()==52,"page bounded 32 with full total 52");
printf("RESULT checks=%d failures=%d disk_reads=%ld disk_writes=%ld syncs=%ld\n",checks,failures,reads,writes,syncs);f_mount(nullptr,"0:",0);fclose(image);return failures?1:0;}
