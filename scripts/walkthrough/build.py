"""scripts/walkthrough/build.py <walkthrough.txt> <objects.jsonl> <monsters.jsonl> <out.html> [move.wav]: the
walkthrough page (docs/walkthrough.html).
objects.jsonl is written by scripts/items/dump.cpp, monsters.jsonl by scripts/bestiary/dump.cpp.

Markup: "@ 篇章 | 標題" starts a section, "# 小標" a heading, "- " a list line, "1. " a step, other lines paragraphs;
"   1. " (indented) a line of a numbered list inside the list line or step above it.
{名} names an item (物品大全), 〔名〕 a monster (神魔異事錄); [標籤] marks a tag; «標題» goes to a section;
【文字｜網址】 is a link out. Unknown names, shop prices the item data does not confirm ("{名} 價" in a 商店 heading)
and steals the monster data does not confirm ("〔怪〕可偷{物}") are reported.
"""
import base64
import html
import json
import re
import sys

SERIES = '系列任務'
TAGS = {'DOMO': 'domo', '藥材': 'herb', '檮杌': 'beast', '饕餮': 'beast', '渾沌': 'beast'}
# Database::LoadObject: the items' ids 0xBB8..0xD48 read the records 0x9C4 lower; the items page numbers them so
# (scripts/items/build.py), the bestiary by their records.
ITEM_ID_SHIFT = 0x9C4
FIRST_ITEM_RECORD = 501
LAST_ITEM_RECORD = 898
FIRST_LIVING_RECORD = 101
ESCORT_KIND = 0x800  # ObjectData::kind of the living things the items page lists with the items


def load(path):
    with open(path, encoding='utf-8') as lines:
        return [json.loads(line) for line in lines if line.strip()]


def sound_url(path):
    """The cursor sound (scripts/sound/extract.py all.snd 0x2E) as a data address; none without one."""
    if not path:
        return None
    with open(path, 'rb') as sound:
        return 'data:audio/wav;base64,' + base64.b64encode(sound.read()).decode('ascii')


def main():
    source, objects_path, monsters_path, out_path, *rest = sys.argv[1:]
    items = {}
    prices = {}
    for row in load(objects_path):
        record = row['id']
        if FIRST_ITEM_RECORD <= record <= LAST_ITEM_RECORD:
            items.setdefault(row['name'], record + ITEM_ID_SHIFT)
            prices.setdefault(row['name'], row['price'])
        elif FIRST_LIVING_RECORD <= record < FIRST_ITEM_RECORD and row['kind'] & ESCORT_KIND:
            items.setdefault(row['name'], record)
    # A name kept by several records (獨孤寧珂 9 and 10) links to the first one fought (with hit points); a steal is
    # confirmed by any of them.
    monsters = {}
    steals = {}
    for row in load(monsters_path):
        if row['name']:
            if row['name'] not in monsters or (row['hp'] and not monsters[row['name']][1]):
                monsters[row['name']] = (row['id'], row['hp'])
            steals.setdefault(row['name'], set()).add(row['steal'])
    monsters = {name: ident for name, (ident, _) in monsters.items()}

    problems = []
    sections = []
    goes = []
    heading = ''
    for number, line in enumerate(open(source, encoding='utf-8'), 1):
        line = line.rstrip('\n')
        if not line.strip():
            continue
        if line.startswith('@ '):
            chapter, title = [part.strip() for part in line[2:].split('|')]
            sections.append(dict(chapter=chapter, title=title, blocks=[], text=[]))
            heading = ''
            continue
        section = sections[-1]
        section['text'].append(re.sub(r'[{}〔〕\[\]«»]', '', line))
        if line.startswith('# '):
            heading = line[2:]

        for monster, item in re.findall(r'〔([^〕]+)〕可偷\{([^}]+)\}', line):
            if monster in steals and item not in steals[monster]:
                problems.append(f'{number}: 〔{monster}〕 steals {sorted(steals[monster])}, text says {item}')

        if heading.startswith('商店') and line.startswith('- '):
            for name, price in re.findall(r'\{([^}]+)\} (\d+)', line):
                if name in prices and prices[name] != int(price):
                    problems.append(f'{number}: {name} costs {prices[name]}, text says {price}')

        def link(match):
            name = match.group(1) or match.group(2)
            monster = match.group(2) is not None
            if monster and name in monsters:
                view, ident = 'bestiary', monsters[name]
            elif not monster and name in items:
                view, ident = 'items', items[name]
            elif name in monsters:
                view, ident = 'bestiary', monsters[name]
            elif name in items:
                view, ident = 'items', items[name]
            else:
                problems.append(f'{number}: unknown {name}')
                return html.escape(name)
            title = '神魔異事錄' if view == 'bestiary' else '物品大全'
            return (f'<span class="jump {view}" data-view="{view}" data-detail="{html.escape(name)}@{ident}" '
                    f'title="在{title}中打開">{html.escape(name)}</span>')

        def tag(match):
            name = match.group(1)
            if name not in TAGS:
                problems.append(f'{number}: unknown tag {name}')
                return html.escape(match.group(0))
            return f'<span class="tag {TAGS[name]}">{html.escape(name)}</span>'

        def render(text):
            parts = re.split(r'(\{[^}]+\}|〔[^〕]+〕|\[[^\]]+\]|«[^»]+»|【[^】]+】)', text)
            out = []
            for part in parts:
                if part.startswith('【') and part.endswith('】') and '｜' in part:
                    label, url = part[1:-1].split('｜', 1)
                    out.append(f'<a class="out" href="{html.escape(url)}" target="_blank" rel="noopener">'
                               f'{html.escape(label)}</a>')
                elif part.startswith('«') and part.endswith('»'):
                    goes.append((number, part[1:-1]))
                    out.append(f'<span class="goto" data-title="{html.escape(part[1:-1])}">'
                               f'{html.escape(part[1:-1])}</span>')
                elif (part.startswith('{') and part.endswith('}')) or (part.startswith('〔') and part.endswith('〕')):
                    out.append(link(re.match(r'\{([^}]+)\}|〔([^〕]+)〕', part)))
                elif part.startswith('[') and part.endswith(']'):
                    out.append(tag(re.match(r'\[([^\]]+)\]', part)))
                else:
                    out.append(html.escape(part))
            return ''.join(out)

        inner = re.match(r'\s+(?:(\d+)\.|-) (.*)', line)
        if inner and not inner.group(1):
            problems.append(f'{number}: an indented line not numbered')
        if inner and section['blocks'] and section['blocks'][-1][0] in ('li', 'step', 'sub-step'):
            section['blocks'].append(['sub-step', render(inner.group(2))])
        elif inner:
            problems.append(f'{number}: an indented line under no list line')
        elif line.startswith('# '):
            section['blocks'].append(['h', render(line[2:])])
        elif line.startswith('- '):
            section['blocks'].append(['li', render(line[2:])])
        elif re.match(r'\d+\. ', line):
            section['blocks'].append(['step', render(line.split('. ', 1)[1])])
        else:
            section['blocks'].append(['p', render(line)])

    titles = {section['title'] for section in sections}
    for number, title in goes:
        if title not in titles:
            problems.append(f'{number}: no section {title}')

    # The side quests that run through the game come after the story's chapters.
    chapters = []
    for section in sections:
        if section['chapter'] not in chapters:
            chapters.append(section['chapter'])
    chapters.sort(key=lambda name: name == SERIES)
    sections.sort(key=lambda section: chapters.index(section['chapter']))

    data = dict(chapters=chapters, sections=[
        dict(chapter=s['chapter'], title=s['title'], text='\n'.join([s['title']] + s['text']),
             html=blocks(s['blocks'])) for s in sections])
    page = TEMPLATE.replace('/*DATA*/null', json.dumps(data, ensure_ascii=False))
    page = page.replace('/*SOUND*/null', json.dumps(sound_url(rest[0] if rest else '')))
    with open(out_path, 'w', encoding='utf-8') as out:
        out.write(page)
    for problem in problems:
        print(problem)
    print(f'{len(sections)} sections, {len(chapters)} chapters, {len(problems)} problems, {len(page)} bytes')


def blocks(rows):
    """The blocks as HTML: list lines and steps in their lists, indented lines in a list inside the line above."""
    lists = {'li': 'ul', 'step': 'ol', 'sub-step': 'ol'}
    out = []
    open_list = None
    open_item = False
    open_inner = None
    for kind, text in rows:
        inner = kind.startswith('sub-')
        if open_inner and (not inner or lists[kind] != open_inner):
            out.append(f'</{open_inner}>')
            open_inner = None
        if inner:
            if open_inner is None:
                open_inner = lists[kind]
                out.append(f'<{open_inner}>')
            out.append(f'<li>{text}</li>')
            continue
        if open_item:
            out.append('</li>')
            open_item = False
        if open_list and lists.get(kind) != open_list:
            out.append(f'</{open_list}>')
            open_list = None
        if kind in lists and open_list is None:
            open_list = lists[kind]
            out.append(f'<{open_list}>')
        if kind in lists:
            out.append(f'<li>{text}')
            open_item = True
        else:
            out.append(f'<h3>{text}</h3>' if kind == 'h' else f'<p>{text}</p>')
    if open_inner:
        out.append(f'</{open_inner}>')
    if open_item:
        out.append('</li>')
    if open_list:
        out.append(f'</{open_list}>')
    return ''.join(out)


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>流程攻略</title>
<style>
:root {
  --gold: #a89048;
  --gold-dark: #6a5420;
  --gold-light: #f0e848;
  --text: #c8b888;
  --dim: #988a62;
  --panel: #100f08;
  --panel-list: #181010;
  --ink: #3a2008;
  --jade-light: #a8d890;
  --kai: "BiauKai", "DFKai-SB", "標楷體", "KaiTi", "STKaiti", "Kaiti TC", serif;
  --ming: "PMingLiU", "MingLiU", "新細明體", "Songti TC", "Noto Serif TC", "Noto Serif CJK TC", serif;
}

* { box-sizing: border-box; }

html, body { margin: 0; height: 100%; overflow: hidden; }

body {
  font-family: var(--ming);
  color: var(--text);
  background:
    radial-gradient(ellipse at 15% 20%, rgba(255, 220, 150, 0.45), transparent 50%),
    radial-gradient(ellipse at 85% 80%, rgba(110, 50, 10, 0.4), transparent 55%),
    radial-gradient(ellipse at 50% 55%, rgba(150, 80, 20, 0.25), transparent 45%),
    radial-gradient(ellipse at 5% 90%, rgba(90, 40, 10, 0.35), transparent 40%),
    repeating-linear-gradient(90deg, rgba(90, 40, 10, 0.12) 0 1px, transparent 1px 80px),
    linear-gradient(160deg, #e0b070 0%, #cc9450 45%, #b07834 100%);
}

.book {
  position: absolute;
  left: 0;
  top: 0;
  width: 1180px;
  height: 870px;
  padding: 12px 12px 8px;
  display: flex;
  flex-direction: column;
  transform-origin: 0 0;
}

.top {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  border-top: 2px solid rgba(255, 230, 180, 0.5);
  background: repeating-linear-gradient(90deg, rgba(255, 230, 180, 0.3) 0 1px, transparent 1px 80px);
  height: 64px;
  flex: none;
  padding: 0 12px 4px 6px;
}

h1 {
  margin: 0;
  font-family: var(--kai);
  font-size: 44px;
  letter-spacing: 6px;
  color: var(--ink);
  text-shadow: 2px 2px 0 rgba(255, 230, 180, 0.45), 0 0 8px rgba(255, 230, 180, 0.35);
  font-weight: bold;
}

.tools { display: flex; align-items: flex-end; gap: 10px; }

.search, .sound {
  height: 34px;
  margin: 0 0 6px;
  font: inherit;
  font-size: 18px;
  color: var(--text);
  background: var(--panel);
  border: 2px solid var(--gold);
  box-shadow: 0 0 0 1px #000, inset 0 0 0 1px #000;
  outline: none;
}

.search { width: 360px; margin-left: 6px; padding: 0 10px; }
.search::placeholder { color: var(--dim); }
.search:focus { border-color: var(--gold-light); }
.sound { padding: 0 12px; font-family: var(--kai); cursor: pointer; white-space: nowrap; }
.sound:hover { border-color: var(--gold-light); }
.sound.off { color: var(--dim); }

.layout {
  display: grid;
  grid-template-columns: 290px 1fr;
  grid-template-rows: 40px 1fr;
  gap: 0 14px;
  margin-top: 8px;
  flex: 1;
  min-height: 0;
}

/* The bright corner marks of the game's frames, inside the frame line. */
.frame {
  --corner: linear-gradient(var(--gold-light), var(--gold-light));
  --fill: var(--panel);
  background:
    var(--corner) left top / 9px 3px no-repeat,
    var(--corner) left top / 3px 9px no-repeat,
    var(--corner) right top / 9px 3px no-repeat,
    var(--corner) right top / 3px 9px no-repeat,
    var(--corner) left bottom / 9px 3px no-repeat,
    var(--corner) left bottom / 3px 9px no-repeat,
    var(--corner) right bottom / 9px 3px no-repeat,
    var(--corner) right bottom / 3px 9px no-repeat,
    var(--fill);
  background-origin: padding-box;
  border: 2px solid var(--gold);
  box-shadow: inset 0 0 0 1px #000, 0 0 0 1px #000;
  position: relative;
}

.tabs {
  grid-column: 1 / span 2;
  display: flex;
  gap: 2px;
  padding-left: 12px;
  margin-bottom: -3px;
  z-index: 1;
}

.tab {
  font-family: var(--kai);
  font-size: 20px;
  min-width: 64px;
  height: 40px;
  line-height: 38px;
  padding: 0 12px;
  color: #2c3410;
  background: linear-gradient(180deg, #889c58, #5c7034);
  --edge: #b89838;
  border: 1px solid #000;
  border-bottom: none;
  border-radius: 10px 10px 0 0;
  box-shadow: inset 0 0 0 2px var(--edge);
  cursor: pointer;
  user-select: none;
  text-align: center;
  white-space: nowrap;
}

.tab.chosen {
  color: #181800;
  background: linear-gradient(180deg, #eef488, #b8d050);
  --edge: #b07818;
  font-weight: bold;
}

.tab.found {
  color: #20280c;
  background: linear-gradient(180deg, #a8bc68, #7c9444);
  --edge: #d0b040;
  box-shadow: inset 0 0 0 2px var(--edge), inset 0 6px 0 #e0c048;
}

.list {
  min-height: 0;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable both-edges;
  padding: 12px 0;
  scrollbar-color: var(--gold) #000;
  --fill: var(--panel-list);
}

.row {
  font-family: var(--kai);
  font-size: 23px;
  line-height: 38px;
  padding: 0 10px 0 16px;
  cursor: pointer;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  text-shadow: 1px 1px 0 #302018;
}

/* The game's highlight: a band brightest in the middle, fading to the top and bottom edges. */
.row.chosen {
  color: #fff0b0;
  background: linear-gradient(180deg, rgba(160, 104, 0, 0), rgba(200, 130, 10, 0.95) 50%, rgba(160, 104, 0, 0));
}

.empty { font-family: var(--kai); font-size: 20px; color: var(--dim); padding: 8px 16px; }

.page { display: flex; flex-direction: column; min-height: 0; padding: 14px 8px 12px 24px; }

.page .title {
  flex: none;
  font-family: var(--kai);
  font-size: 32px;
  line-height: 44px;
  color: #f0e0a8;
  border-bottom: 1px solid var(--gold-dark);
  margin-right: 16px;
  padding-bottom: 4px;
}

.page .title small { font-size: 18px; color: var(--dim); margin-left: 14px; }

.page .body {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  scrollbar-gutter: stable;
  padding: 4px 14px 10px 0;
  scrollbar-color: var(--gold) #000;
  font-size: 18px;
  line-height: 31px;
}

.page h3 { font-family: var(--kai); font-size: 23px; font-weight: normal; color: var(--gold-light); margin: 14px 0 2px; }
.page p { margin: 2px 0 6px; }
.page ul, .page ol { margin: 0 0 6px; padding-left: 26px; }
.page li > ul, .page li > ol { margin: 0 0 2px; padding-left: 24px; }
.page ol li::marker { font-size: 16px; }
.page li::marker { color: var(--gold); }

.jump { color: #f0e0a8; cursor: pointer; border-bottom: 1px dotted rgba(240, 232, 72, 0.5); }
.jump.bestiary { color: #f0c8a0; }
.jump:hover { color: var(--gold-light); border-bottom-style: solid; }
.goto { color: var(--jade-light); cursor: pointer; border-bottom: 1px dotted rgba(168, 216, 144, 0.6); }
.goto:hover { border-bottom-style: solid; }
.out { color: var(--gold-light); }
.out:hover { color: #fff0b0; }

.tag {
  display: inline-block;
  font-family: var(--kai);
  font-size: 15px;
  line-height: 21px;
  padding: 0 7px;
  margin: 0 3px;
  border-radius: 9px;
  vertical-align: 1px;
  white-space: nowrap;
}

.tag.domo { color: #fff0c0; background: linear-gradient(180deg, #4a7a3a, #24401c); border: 1px solid var(--jade-light); }
.tag.herb { color: #fff0c0; background: linear-gradient(180deg, #8a6a1c, #4a3608); border: 1px solid var(--gold); }
.tag.beast { color: #ffd8c8; background: #4a1e14; border: 1px solid #a05a40; }

mark { background: rgba(232, 168, 56, 0.45); color: inherit; }

.hint { flex: none; margin-top: 6px; font-size: 13px; color: #3a2008; text-align: right; }
</style>
</head>
<body>
<div class="book">
  <div class="top">
    <div class="tools">
      <input class="search" id="search" type="search" placeholder="搜尋地點、物品、支線" autocomplete="off">
      <button class="sound" id="sound" type="button"></button>
    </div>
    <h1>流程攻略</h1>
  </div>
  <div class="layout">
    <div class="tabs" id="tabs"></div>
    <div class="frame list" id="list"></div>
    <div class="frame page"><div class="title" id="title"></div><div class="body" id="body"></div></div>
  </div>
  <div class="hint">↑↓ 選擇　Home / End 首尾　←→ 切換篇章　點物品或怪物的名字可打開物品大全、神魔異事錄，點綠色的條目名跳到那一頁　搜尋可空格分隔多個關鍵字，Esc 清除</div>
</div>
<script>
const DATA = /*DATA*/null;
const CHAPTERS = DATA.chapters;
const SECTIONS = DATA.sections;

// The game's cursor sound (all.snd 0x2E, which the menus play as the cursor moves).
const MOVE_SOUND = /*SOUND*/null;

// The sound is on unless the address says ?sound=off; the switch keeps it there (and, shown in the home page, tells
// the home page, which keeps it in its own address and passes it to the other pages).
const SOUND_PARAMETER = 'sound';
const SOUND_OFF = 'off';
let soundOn = new URLSearchParams(location.search).get(SOUND_PARAMETER) !== SOUND_OFF;

function beep() {
  if (MOVE_SOUND && soundOn) {
    const sound = new Audio(MOVE_SOUND);
    sound.volume = 0.6;
    sound.play().catch(() => {});
  }
}

let chapter = 0;
let chosen = 0;
let shown = [];

const $ = id => document.getElementById(id);

function escape(text) {
  return text.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
}

function query() {
  return $('search').value.trim().toLowerCase().split(/\s+/).filter(word => word);
}

function drawTabs() {
  $('tabs').innerHTML = CHAPTERS.map((name, at) =>
    `<div class="tab${at === chapter ? ' chosen' : ''}" data-at="${at}">${escape(name)}</div>`).join('');
}

function list() {
  const words = query();
  shown = words.length ? SECTIONS.filter(section => words.every(word => section.text.toLowerCase().includes(word))) :
    SECTIONS.filter(section => section.chapter === CHAPTERS[chapter]);
  chosen = Math.min(chosen, Math.max(shown.length - 1, 0));
  $('list').innerHTML = shown.length ? shown.map((section, at) =>
    `<div class="row${at === chosen ? ' chosen' : ''}" data-at="${at}">${escape(section.title)}</div>`).join('') :
    '<div class="empty">找不到</div>';
  show();
}

// The words searched marked in the text of the page (not inside its tags).
function highlight(root, words) {
  if (!words.length) {
    return;
  }

  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
  const nodes = [];

  while (walker.nextNode()) {
    nodes.push(walker.currentNode);
  }

  const pattern = new RegExp(words.map(word => word.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|'), 'gi');

  for (const node of nodes) {
    if (!pattern.test(node.data)) {
      continue;
    }

    pattern.lastIndex = 0;
    const span = document.createElement('span');
    span.innerHTML = escape(node.data).replace(pattern, found => `<mark>${found}</mark>`);
    node.replaceWith(...span.childNodes);
  }
}

function show() {
  document.querySelectorAll('.row').forEach((row, at) => row.classList.toggle('chosen', at === chosen));
  const row = document.querySelector('.row.chosen');

  if (row) {
    row.scrollIntoView({ block: 'nearest' });
  }

  const section = shown[chosen];
  const words = query();
  document.querySelectorAll('.tab').forEach((tab, at) => {
    tab.classList.toggle('chosen', !words.length && at === chapter);
    tab.classList.toggle('found', words.length > 0 && section !== undefined && CHAPTERS[at] === section.chapter);
  });

  if (!section) {
    $('title').innerHTML = '';
    $('body').innerHTML = '';
    return;
  }

  keepPlace(section.title);
  $('title').innerHTML = `${escape(section.title)}<small>${escape(section.chapter)}</small>`;
  $('body').innerHTML = section.html;
  highlight($('body'), words);
  const mark = $('body').querySelector('mark');
  $('body').scrollTop = mark ? mark.offsetTop - $('body').offsetTop - 60 : 0;
}

function move(by) {
  if (!shown.length) {
    return;
  }

  const before = chosen;
  chosen = Math.max(0, Math.min(shown.length - 1, chosen + by));

  if (chosen !== before) {
    beep();
  }

  show();
}

function turn(by) {
  beep();
  chapter = (chapter + by + CHAPTERS.length) % CHAPTERS.length;
  chosen = 0;
  $('search').value = '';
  drawTabs();
  list();
}

$('search').addEventListener('input', () => {
  chosen = 0;
  list();
});

$('tabs').addEventListener('click', event => {
  const tab = event.target.closest('.tab');

  if (tab) {
    turn(Number(tab.dataset.at) - chapter);
  }
});

$('list').addEventListener('click', event => {
  const row = event.target.closest('.row');

  if (row) {
    if (Number(row.dataset.at) !== chosen) {
      beep();
    }

    chosen = Number(row.dataset.at);
    show();
  }
});

// A name opens the items page or the bestiary on it: the home page shows it beside its menu, alone the page itself.
$('body').addEventListener('click', event => {
  const goto = event.target.closest('.goto');

  if (goto) {
    chooseNamed(goto.dataset.title);
    return;
  }

  const jump = event.target.closest('.jump');

  if (!jump) {
    return;
  }

  const { view, detail } = jump.dataset;

  if (window.parent !== window) {
    window.parent.postMessage({ open: view, detail }, '*');
    return;
  }

  location.href = `${view}.html${location.search}#${encodeURIComponent(detail)}`;
});

document.addEventListener('keydown', event => {
  const typing = event.target === $('search');

  if (typing && event.key === 'Escape') {
    $('search').value = '';
    $('search').dispatchEvent(new Event('input'));
    return;
  }

  if (typing && ['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) {
    return;
  }

  const keys = {
    ArrowUp: () => move(-1),
    ArrowDown: () => move(1),
    Home: () => move(-shown.length),
    End: () => move(shown.length),
    ArrowLeft: () => turn(-1),
    ArrowRight: () => turn(1),
    PageUp: () => { $('body').scrollTop -= $('body').clientHeight - 40; },
    PageDown: () => { $('body').scrollTop += $('body').clientHeight - 40; },
  };

  if (keys[event.key]) {
    keys[event.key]();
    event.preventDefault();
  }
});

// The book keeps its size: scaled whole to fit the window, centred.
function fit() {
  const book = document.querySelector('.book');
  const scale = Math.min(innerWidth / book.offsetWidth, innerHeight / book.offsetHeight);
  const left = (innerWidth - book.offsetWidth * scale) / 2;
  const top = (innerHeight - book.offsetHeight * scale) / 2;
  book.style.transform = `translate(${left}px, ${top}px) scale(${scale})`;
}

// The section shown kept in the address (and in the home page's) in place, so that going back returns here.
function keepPlace(detail) {
  history.replaceState(null, '', `${location.search}#${encodeURIComponent(detail)}`);

  if (window.parent !== window) {
    window.parent.postMessage({ shown: 'walkthrough', detail }, '*');
  }
}

function addressed() {
  try {
    return decodeURIComponent(location.hash.slice(1));
  } catch {
    return '';
  }
}

// A section named in the address (walkthrough.html#月河村) is chosen in its chapter.
function chooseNamed(text = addressed()) {
  const section = SECTIONS.find(each => each.title === text);

  if (!section) {
    return;
  }

  chapter = CHAPTERS.indexOf(section.chapter);
  $('search').value = '';
  drawTabs();
  list();
  chosen = shown.indexOf(section);
  show();
}

function drawSound() {
  const button = document.getElementById('sound');
  button.textContent = soundOn ? '音效 開' : '音效 關';
  button.classList.toggle('off', !soundOn);
}

function setSound(on, fromHome = false) {
  soundOn = on;
  const query = new URLSearchParams(location.search);

  if (soundOn) {
    query.delete(SOUND_PARAMETER);
  } else {
    query.set(SOUND_PARAMETER, SOUND_OFF);
  }

  const text = query.toString();
  history.replaceState(null, '', `${location.pathname}${text ? `?${text}` : ''}${location.hash}`);
  drawSound();

  if (!fromHome && window.parent !== window) {
    window.parent.postMessage({ sound: soundOn }, '*');
  }
}

document.getElementById('sound').addEventListener('click', event => {
  setSound(!soundOn);
  beep();
  event.currentTarget.blur();
});

addEventListener('message', event => {
  const { sound } = event.data || {};

  if (typeof sound === 'boolean' && event.source === window.parent) {
    setSound(sound, true);
  }
});

drawSound();
addEventListener('resize', fit);
addEventListener('hashchange', () => chooseNamed());
const place = addressed();
fit();
drawTabs();
list();
chooseNamed(place);
</script>
</body>
</html>
'''

if __name__ == '__main__':
    main()
