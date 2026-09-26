#include "writer_core.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

static size_t prev(const std::string& s, size_t p) {
    if(!p) return 0;
    do {--p;} while(p && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80);
    return p;
}
static size_t next(const std::string& s, size_t p) {
    if(p == s.size()) return p;
    ++p;
    while(p < s.size() && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80) ++p;
    return p;
}
// The piece table refuses an edit only when typed text or the piece list is full.
static bool full(const writer::TextModel& model, size_t n, int pieces) {
    return model.unsaved_bytes() + n > writer::TEXT_CAPACITY || model.pieces() + pieces > writer::MAX_PIECES;
}
int main() {
    writer::TextModel model;
    std::string ref;
    size_t caret = 0;
    std::mt19937 rng(0x475741);
    const std::vector<std::string> alphabet = {"a", "Z", " ", "\n", "é", "ä", "ç", "ñ", "ø", "œ", "ß", "ž", "Ÿ", "🙂", "\t", "\r\n"};
    for(unsigned step = 0; step < 100000; ++step) {
        const auto& value = alphabet[rng() % alphabet.size()];
        switch(rng() % 9) {
        case 0: case 1: {
            bool allowed = model.insert(value.c_str());
            assert(allowed || full(model, value.size(), 2));
            if(allowed) {ref.insert(caret, value); caret += value.size();}
            break;
        }
        case 2: {
            size_t p = prev(ref, caret);
            bool done = model.backspace();
            assert(done == (caret != 0) || (caret && full(model, 0, 1)));
            if(done) {ref.erase(p, caret-p); caret = p;}
            break;
        }
        case 3: model.move_left(); caret = prev(ref, caret); break;
        case 4: model.move_right(); caret = next(ref, caret); break;
        case 5: model.move_home(); caret=0; break;
        case 6: model.move_end(); caret=ref.size(); break;
        case 7: {
            size_t p = prev(ref, caret);
            bool allowed = model.replace_before_caret(value.c_str());
            assert(allowed || full(model, value.size(), 3)); // a replaced file byte frees no room
            if(allowed) {ref.replace(p, caret-p, value); caret=p+value.size();}
            break;
        }
        case 8:
            if(step % 100 == 0) {
                ref.assign(writer::TEXT_CAPACITY, 'x');
                assert(model.set_text(ref.c_str()));
                caret=ref.size();
            } else {
                std::string before=model.str().c_str();
                auto old_caret=model.caret_byte();
                assert(!model.insert("\xc0\xaf"));
                assert(!model.set_text("\xed\xa0\x80"));
                assert(before==model.str().c_str() && old_caret==model.caret_byte());
            }
        }
        assert(model.bytes() == ref.size());
        assert(model.caret_byte() == caret);
        assert(ref == model.str().c_str());
        assert(writer::valid_utf8(model.str().c_str(), model.bytes()));
    }
    std::cout << "PASS: 100000 deterministic TextModel differential operations, Unicode and capacity boundaries\n";
}
