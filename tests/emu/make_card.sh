#!/bin/sh
# Test card image for writer_runner: FAT16, /gbawriter with diary files and
# notes, plus folders elsewhere on the card for Import.
set -e
img=$1
rm -f "$img"
dd if=/dev/zero of="$img" bs=1M count=64 status=none
mkfs.fat -F 16 -n GBAWRITER "$img" >/dev/null
t=$(mktemp -d)
printf 'Dear diary, day one.\n' > "$t/a"
printf 'Second day.\n' > "$t/b"
printf 'Shopping: bread, milk, a very long list of things to remember.\n' > "$t/c"
printf 'Imported text.\n' > "$t/d"
export MTOOLS_SKIP_CHECK=1
mmd -i "$img" ::/gbawriter ::/Books ::/Books/Old ::/Music
mcopy -i "$img" "$t/a" ::/gbawriter/09072026.txt
mcopy -i "$img" "$t/b" ::/gbawriter/10072026.txt
mcopy -i "$img" "$t/c" "::/gbawriter/Shopping list with a really long name for the marquee.txt"
mcopy -i "$img" "$t/c" ::/gbawriter/Notes.txt
mcopy -i "$img" "$t/d" ::/Books/Story.txt
mcopy -i "$img" "$t/d" ::/Books/Notes.txt
mcopy -i "$img" "$t/d" ::/Books/Old/Letter.txt
mcopy -i "$img" "$t/d" ::/Music/song.mp3
mcopy -i "$img" "$t/d" ::/readme.txt
rm -rf "$t"
