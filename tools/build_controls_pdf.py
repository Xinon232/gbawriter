#!/usr/bin/env python3
"""Build the full-controls PDF from the maintained README (requires reportlab)."""
from pathlib import Path
import re
import sys
from xml.sax.saxutils import escape
from reportlab import rl_config
rl_config.invariant = 1
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Flowable

root = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'gbawriter-full-controls.pdf'
fontdir = Path('/usr/share/fonts/truetype/dejavu')
for name, filename in [('Body','DejaVuSans.ttf'), ('BodyBold','DejaVuSans-Bold.ttf')]:
    pdfmetrics.registerFont(TTFont(name, str(fontdir / filename)))
pdfmetrics.registerFontFamily('Body', normal='Body', bold='BodyBold', italic='Body', boldItalic='BodyBold')
styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyTextUTF', fontName='Body', fontSize=9, leading=13, spaceAfter=7))
styles.add(ParagraphStyle(name='SectionUTF', fontName='BodyBold', fontSize=13, leading=17, textColor=colors.HexColor('#174a8b'), spaceBefore=12, spaceAfter=8, keepWithNext=True))
styles.add(ParagraphStyle(name='TitleUTF', fontName='BodyBold', fontSize=23, leading=28, textColor=colors.HexColor('#174a8b'), spaceAfter=14))
styles.add(ParagraphStyle(name='CellUTF', fontName='Body', fontSize=8, leading=11))

def markup(text):
    text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', text)
    text = escape(text)
    text = re.sub(r'\*\*(.*?)\*\*', r'<b>\1</b>', text)
    return text.replace('`', '')

readme = (root / 'README.md').read_text()
# Include every maintained file/menu/typing/navigation/save control, not a shortcut summary.
body = readme.split('## Hardware and installation\n', 1)[1].split('## Text, memory and limits', 1)[0]
story: list[Flowable] = [Paragraph('gbawriter V4.2', styles['TitleUTF']),
         Paragraph('Full controls · Halim Jarrar', styles['SectionUTF']),
         Paragraph('Create and edit TXT files on your Game Boy Advance. Save your writing directly to the SD card. Put TXT files in <b>/gbawriter</b> at the root of your SD card, or import them from anywhere on the card (Start on Home). Requires a compatible Supercard SD.', styles['BodyTextUTF']),
         Paragraph('Hardware-unverified build: emulator and host tests are not proof of safe saving on a physical Supercard. Back up your SD card and use disposable documents first.', styles['BodyTextUTF']),
         Paragraph('Installation', styles['SectionUTF'])]
lines = body.splitlines()
i = 0
while i < len(lines):
    line = lines[i].strip()
    if not line or line.startswith('The [full-controls PDF]'):
        i += 1
        continue
    if line.startswith('|'):
        rows = []
        while i < len(lines) and lines[i].strip().startswith('|'):
            row = [c.strip() for c in lines[i].strip().strip('|').split('|')]
            if not all(re.fullmatch(r'[-:]+', c) for c in row):
                rows.append([Paragraph(markup(c), styles['CellUTF']) for c in row])
            i += 1
        columns = len(rows[0])
        widths = [110, 175, 190] if columns == 3 else [150, 325]
        table = Table(rows, colWidths=widths, repeatRows=1, hAlign='LEFT')
        table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#eaf1fa')),('VALIGN',(0,0),(-1,-1),'TOP'),('LINEBELOW',(0,0),(-1,0),0.6,colors.HexColor('#174a8b')),('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5)]))
        story.extend([table, Spacer(1, 9)])
        continue
    if line.startswith('#'):
        story.append(Paragraph(markup(line.lstrip('#').strip()), styles['SectionUTF']))
    else:
        if line.startswith('- '):
            line = '• ' + line[2:]
        story.append(Paragraph(markup(line), styles['BodyTextUTF']))
    i += 1
story.extend([Paragraph('Storage and attribution', styles['SectionUTF']),
    Paragraph('Files have no size limit: they stay on the SD card and only changes use RAM. Up to 64 KiB of typed text can be unsaved at once; when the bar shows BUFFER FULL - SAVE, save to continue (the text is kept). Saving a large file takes longer. Saves change only the opened TXT file. Settings and the list order are kept in the hidden folder /gbawriter/GBAWRITER.SYS. Temporary .gwt, .gwb and .gwi recovery files may remain after interrupted or failed saves. Do not delete them on the original card: back up the card and consult the README recovery instructions. There is no undo/redo or autosave: switching off loses text that is not saved.', styles['BodyTextUTF']),
    Paragraph('Made by Halim Jarrar · (C) 2026 · halimj.itch.io · gba@halim-jarrar.de. gbawriter is free software under the GNU GPL 3.0 or later. Text renderer: SuperFW by David Guillen Fandos (GPL 3.0 or later). Fonts: UNSCII by Viznut (GPL) and GNU Unifont, also its Hangul blocks (GPL 2.0 or later). UI font: from gbamp3 (MIT). SD card driver: SuperFW (GPL 3.0 or later). File system: FatFs by ChaN (BSD style). Engine: Butano by Gustavo Valiente (zlib); built with devkitPro / devkitARM. Font and vendor notices remain in the source; see LICENSE and README.', styles['BodyTextUTF'])])

def footer(canvas, doc):
    canvas.setFont('Body', 8)
    canvas.setFillColor(colors.HexColor('#526174'))
    canvas.drawString(60, 30, 'gbawriter V4.2 · Full controls · Halim Jarrar')
    canvas.drawRightString(535, 30, str(doc.page))

SimpleDocTemplate(str(output), pagesize=(595.28,841.89), rightMargin=60, leftMargin=60, topMargin=48, bottomMargin=52, title='gbawriter V4.2 — Full controls', author='Halim Jarrar').build(story, onFirstPage=footer, onLaterPages=footer)
print(output)
