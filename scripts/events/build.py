"""scripts/events/build.py <events.jsonl> <objects.jsonl> <formations.jsonl> <out.html>: the events page
(docs/events.html), silent.

events.jsonl: scripts/events/dump (the place names, the roles and regions with an event, every event's commands and
dialogs); objects.jsonl: scripts/items/dump data (the names of mon.dat's objects, the items and the enemies);
formations.jsonl: scripts/bestiary/dump formations (the enemies of each battle).

Each event: its number, where it is set off (a role of maps.dat on a map, a region of a map, or another event that
opens it or gives it to a role), the maps it changes to, its battles with their enemies, the items and gold it gives
or takes, the events it opens or gives to roles, and its dialogs. The events are in four tabs, one per Talk file, each
listed whole; the search finds a number, a map, a name or a line among them all. An event of nothing but
the interpreter's switches is left out unless a role, a region or another event sets it off.
"""
import html
import json
import re
import sys

TALK_EVENTS = 2000  # events per Talk file (Talk::EventsPerFile)
TALK_FILES = 4
ITEM_BASE = 0x9C4  # an item's id is its mon.dat record plus this
GOLD = 0  # the variable holding the gold (Party::Gold)
CHEST_ACTS = {625: '寶箱', 626: '寶箱', 622: '隱藏物品'}  # the acts of the chests and hidden things
# The commands read (talk/event_command.h).
CHANGE_MAP = 0x1B
SET_ROLE_TALK = 0x1C
ADD_VARIABLE = 0x1E
SUBTRACT_VARIABLE = 0x1F
SET_EVENT = 0x64
START_BATTLE = 0x58
ADD_ITEM = 0x80
OPEN_SCRIPT = 0xA1
ADD_VARIABLE_WIDE = 0xB6
FILE_LABELS = ['第一卷', '第二卷', '第三卷', '第四卷']
# The interpreter's own switches (RunOn, RunOff, SkipQuick): an event of nothing else does nothing.
SWITCHES = 0x400


def signed(word):
    return word - 0x10000 if word >= 0x8000 else word


def main():
    events_path, objects_path, formations_path, out_path = sys.argv[1:]
    names = {}
    for line in open(objects_path, encoding='utf-8'):
        row = json.loads(line)
        names[row['id']] = row['name']
    formations = {}
    for line in open(formations_path, encoding='utf-8'):
        row = json.loads(line)
        formations[row['battle']] = row['enemies']

    places = {}
    roles = []
    regions = []
    events = {}
    for line in open(events_path, encoding='utf-8'):
        row = json.loads(line)
        kind = row['kind']
        if kind == 'map':
            places[row['map']] = row['name']
        elif kind == 'role':
            roles.append(row)
        elif kind == 'region':
            regions.append(row)
        else:
            events[row['event']] = row

    def place(map_number):
        name = places.get(map_number)
        return f'{name}（地圖 {map_number}）' if name else f'地圖 {map_number}'

    def item_name(ident):
        if ident > ITEM_BASE and ident - ITEM_BASE in names:
            return names[ident - ITEM_BASE]
        return names.get(ident, f'物品 {ident}')

    def clean(text):
        """A dialog as shown: the item names put in, the choices bracketed, the other control codes left out."""
        text = re.sub(r'%T(\d+)\.', lambda match: item_name(int(match.group(1))), text)
        text = re.sub(r'%B([^.]*)\.', r'〔\1〕', text)
        text = re.sub(r'%A\d*\.', '', text)
        text = re.sub(r'%[CSDG]\d', '', text)
        text = text.replace('%N', '\n')
        text = re.sub(r'%.', '', text)
        return '\n'.join(part.strip() for part in text.split('\n') if part.strip())

    # Where each event is set off.
    sources = {}
    for role in roles:
        kind = CHEST_ACTS.get(role['act'], '角色')
        sources.setdefault(role['event'], []).append(
            f'{place(role["map"])}的{kind}（編號 {role["id"]}，座標 {role["x"]},{role["y"]}）'
        )
    for region in regions:
        label = f'「{region["name"]}」' if region['name'] else f'區域 {region["region"]}'
        sources.setdefault(region['event'], []).append(f'{place(region["map"])}的地區{label}')

    opened_by = {}
    given_by = {}
    rows = []
    for number in sorted(events):
        event = events[number]
        maps = []
        battles = []
        gains = []
        opens = []
        gives = []
        for command in event['commands']:
            code, operands = command[0], command[1:]
            if code == CHANGE_MAP and operands:
                if operands[0] not in maps:
                    maps.append(operands[0])
            elif code == START_BATTLE and operands:
                battles.append(operands[0])
            elif code == ADD_ITEM and len(operands) >= 2:
                count = signed(operands[1])
                gains.append(f'{item_name(operands[0])} {"+" if count > 0 else ""}{count}')
            elif code in (ADD_VARIABLE, SUBTRACT_VARIABLE) and len(operands) >= 2 and operands[0] == GOLD:
                amount = signed(operands[1]) * (1 if code == ADD_VARIABLE else -1)
                gains.append(f'金錢 {"+" if amount > 0 else ""}{amount}')
            elif code == ADD_VARIABLE_WIDE and len(operands) >= 3 and operands[0] == GOLD:
                amount = operands[1] | operands[2] << 16
                amount = amount - 0x100000000 if amount >= 0x80000000 else amount
                gains.append(f'金錢 {"+" if amount > 0 else ""}{amount}')
            elif code == OPEN_SCRIPT and operands:
                if operands[0] not in opens:
                    opens.append(operands[0])
                opened_by.setdefault(operands[0], []).append(number)
            elif code in (SET_ROLE_TALK, SET_EVENT) and len(operands) >= 2 and operands[1] != 0:
                if operands[1] not in gives:
                    gives.append(operands[1])
                given_by.setdefault(operands[1], []).append(number)

        facts = []
        if maps:
            facts.append(['換地圖', '、'.join(place(map_number) for map_number in maps)])
        if battles:
            shown = []
            for battle in battles:
                enemies = '、'.join(names.get(enemy, str(enemy)) for enemy in formations.get(battle, []))
                shown.append(f'{battle}（{enemies}）' if enemies else str(battle))
            facts.append(['戰鬥', '、'.join(shown)])
        if gains:
            facts.append(['得失', '、'.join(gains)])
        rows.append(dict(
            empty=all(command[0] >= SWITCHES for command in event['commands']) and not event['texts'],
            id=number,
            facts=facts,
            opens=opens,
            gives=gives,
            lines=[line for line in (clean(text) for text in event['texts']) if line],
        ))

    for row in rows:
        number = row['id']
        row['where'] = sources.get(number, [])
        row['openedBy'] = sorted(set(opened_by.get(number, [])))
        row['givenBy'] = sorted(set(given_by.get(number, [])))

    # An event that does nothing is left out unless something sets it off.
    left = len(rows)
    rows = [row for row in rows if not row.pop('empty') or row['where'] or row['openedBy'] or row['givenBy']]
    left -= len(rows)

    tabs = [
        dict(label=FILE_LABELS[file], range=f'{max(file * TALK_EVENTS, 1)}–{(file + 1) * TALK_EVENTS - 1}')
        for file in range(TALK_FILES)
    ]
    data = json.dumps(dict(tabs=tabs, events=rows), ensure_ascii=False, separators=(',', ':'))
    tab_html = ''.join(
        f'<div class="tab" data-at="{at}">{html.escape(tab["label"])}<small>{tab["range"]}</small></div>'
        for at, tab in enumerate(tabs)
    )
    page = TEMPLATE.replace('<!--TABS-->', tab_html).replace('/*DATA*/null', data)
    with open(out_path, 'w', encoding='utf-8') as out:
        out.write(page)
    print(
        f'{len(rows)} events ({left} doing nothing left out), '
        f'{sum(1 for row in rows if row["where"])} set off by roles or regions, {len(page)} bytes'
    )


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>事件一覽</title>
<style>
:root {
  --gold: #a89048;
  --gold-dark: #6a5420;
  --gold-light: #f0e848;
  --text: #c8b888;
  --dim: #988a62;
  --panel: #100f08;
  --ink: #3a2008;
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

.tools { display: flex; align-items: center; gap: 10px; padding-bottom: 6px; }

.tools input {
  font-family: var(--ming);
  font-size: 18px;
  width: 300px;
  padding: 4px 8px;
  color: #f0e0a8;
  background: rgba(16, 15, 8, 0.85);
  border: 2px solid var(--gold);
  outline: none;
}

.tools input::placeholder { color: var(--dim); }

.layout {
  display: grid;
  grid-template-columns: 116px 1fr;
  gap: 0 14px;
  margin-top: 8px;
  flex: 1;
  min-height: 0;
}

.tabs {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding-top: 10px;
  margin-right: -20px;
  z-index: 1;
}

.tab {
  font-family: var(--kai);
  font-size: 19px;
  padding: 6px 19px 6px 12px;
  color: #2c3410;
  background: linear-gradient(90deg, #5c7034, #889c58);
  --edge: #b89838;
  border: 1px solid #000;
  border-right: none;
  border-radius: 10px 0 0 10px;
  box-shadow: inset 0 0 0 2px var(--edge);
  cursor: pointer;
  user-select: none;
  text-align: center;
  white-space: nowrap;
}

.tab small { display: block; font-family: var(--ming); font-size: 13px; }

.tab.chosen {
  color: #181800;
  background: linear-gradient(90deg, #b8d050, #eef488);
  --edge: #b07818;
  font-weight: bold;
}

.layout .page { z-index: 2; }

.frame {
  --corner: linear-gradient(var(--gold-light), var(--gold-light));
  background:
    var(--corner) left top / 9px 3px no-repeat,
    var(--corner) left top / 3px 9px no-repeat,
    var(--corner) right top / 9px 3px no-repeat,
    var(--corner) right top / 3px 9px no-repeat,
    var(--corner) left bottom / 9px 3px no-repeat,
    var(--corner) left bottom / 3px 9px no-repeat,
    var(--corner) right bottom / 9px 3px no-repeat,
    var(--corner) right bottom / 3px 9px no-repeat,
    var(--panel);
  border: 2px solid var(--gold);
  box-shadow: inset 0 0 0 1px #000, 0 0 0 1px #000;
  position: relative;
}

.page { display: flex; flex-direction: column; min-height: 0; padding: 14px 8px 12px 24px; }

.page .title {
  flex: none;
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  font-family: var(--kai);
  font-size: 30px;
  line-height: 42px;
  color: #f0e0a8;
  border-bottom: 1px solid var(--gold-dark);
  margin-right: 16px;
  padding-bottom: 4px;
}

.page .body {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  scrollbar-gutter: stable;
  padding: 4px 14px 10px 0;
  scrollbar-color: var(--gold) #000;
  font-size: 17px;
  line-height: 28px;
}

.event { padding: 10px 0 12px; border-bottom: 1px solid rgba(168, 144, 72, 0.35); }
.event.marked { background: rgba(240, 232, 72, 0.08); }

.event .head { display: flex; gap: 14px; align-items: baseline; }
.event .number { font-family: var(--kai); font-size: 24px; color: var(--gold-light); min-width: 64px; }
.event .where { color: #f0e0a8; overflow-wrap: anywhere; }
.event .where.none { color: var(--dim); }

.event dl { margin: 4px 0 0 78px; display: grid; grid-template-columns: auto 1fr; gap: 0 12px; }
.event dt { font-family: var(--kai); color: var(--gold); white-space: nowrap; }
.event dd { margin: 0; overflow-wrap: anywhere; }

.event .lines { margin: 6px 0 0 78px; color: var(--text); }
.event .lines p { margin: 0 0 4px; white-space: pre-line; overflow-wrap: anywhere; }
.event .lines p::before { content: "「"; color: var(--dim); }
.event .lines p::after { content: "」"; color: var(--dim); }
.event details summary { cursor: pointer; color: var(--gold); font-size: 15px; }

.event a { color: var(--gold-light); text-decoration: none; cursor: pointer; }
.event a:hover { text-decoration: underline; }

.empty { color: var(--dim); padding: 20px 0; }

.hint { flex: none; margin-top: 6px; font-size: 13px; color: #3a2008; text-align: right; }
</style>
</head>
<body>
<div class="book">
  <div class="top">
    <div class="tools"><input id="search" type="search" placeholder="搜尋編號、地圖、物品或台詞"></div>
    <h1>事件一覽</h1>
  </div>
  <div class="layout">
    <div class="tabs" id="tabs"><!--TABS--></div>
    <div class="frame page">
      <div class="title"><span id="heading"></span></div>
      <div class="body" id="body"></div>
    </div>
  </div>
  <div class="hint">編號可填入作弊畫面（調試模式 Ctrl+F4）的號碼欄，按「劇情」直接執行　←→ 分頁　↑↓ PgUp / PgDn 捲動　Home / End 首尾</div>
</div>
<script>
const DATA = /*DATA*/null;
const $ = id => document.getElementById(id);
const byId = new Map(DATA.events.map(event => [event.id, event]));
const tabs = [...document.querySelectorAll('.tab')];
const FILE_EVENTS = 2000;

let shownTab = 0;
let list = [];
let marked = 0;

function fileOf(id) {
  return Math.min(Math.floor(id / FILE_EVENTS), tabs.length - 1);
}

function escaped(text) {
  return text.replace(/[&<>"]/g, character => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[character]);
}

function eventLinks(ids) {
  return ids.map(id => `<a data-event="${id}">${id}</a>`).join('、');
}

// An event as a block: its number and where it is set off, its facts, the events it opens or hands on, its lines.
function eventHtml(event) {
  const where = event.where.length
    ? `<span class="where">${event.where.map(escaped).join('；')}</span>`
    : '<span class="where none">不在地圖上，由其他事件開啟或交給角色</span>';
  const facts = event.facts.map(([label, text]) => [label, escaped(text)]);

  if (event.openedBy.length) {
    facts.push(['開啟它的', eventLinks(event.openedBy)]);
  }

  if (event.givenBy.length) {
    facts.push(['交給角色的', eventLinks(event.givenBy)]);
  }

  if (event.opens.length) {
    facts.push(['開啟', eventLinks(event.opens)]);
  }

  if (event.gives.length) {
    facts.push(['交給角色', eventLinks(event.gives)]);
  }

  const lines = event.lines.map(line => `<p>${escaped(line)}</p>`);
  const shown = lines.slice(0, 3).join('');
  const rest = lines.length > 3
    ? `<details><summary>其餘 ${lines.length - 3} 句台詞</summary>${lines.slice(3).join('')}</details>`
    : '';
  return `<div class="event${event.id === marked ? ' marked' : ''}" id="event${event.id}">`
    + `<div class="head"><span class="number">${event.id}</span>${where}</div>`
    + (facts.length ? `<dl>${facts.map(([label, text]) => `<dt>${label}</dt><dd>${text}</dd>`).join('')}</dl>` : '')
    + (lines.length ? `<div class="lines">${shown}${rest}</div>` : '')
    + '</div>';
}

// What the search finds: the event of a number, else every event whose places, facts or lines hold the words.
function found(text) {
  const words = text.trim().split(/\s+/).filter(Boolean);

  if (words.length === 1 && /^\d+$/.test(words[0]) && byId.has(Number(words[0]))) {
    return [byId.get(Number(words[0]))];
  }

  return DATA.events.filter(event => {
    const all = [event.id, ...event.where, ...event.facts.flat(), ...event.lines].join('\n');
    return words.every(word => all.includes(word));
  });
}

function render(scrollTo) {
  $('body').innerHTML = list.length ? list.map(eventHtml).join('') : '<div class="empty">沒有符合的事件</div>';
  const query = $('search').value.trim();
  $('heading').textContent = query
    ? `搜尋「${query}」：${list.length} 個事件`
    : `${DATA.tabs[shownTab].label}　事件 ${DATA.tabs[shownTab].range}（${list.length} 個）`;
  const target = scrollTo ? $(`event${scrollTo}`) : null;
  $('body').scrollTop = target ? target.offsetTop - $('body').offsetTop - 8 : 0;
}

function showTab(at) {
  shownTab = at;
  tabs.forEach((tab, index) => tab.classList.toggle('chosen', index === at && !$('search').value.trim()));
  list = DATA.events.filter(event => fileOf(event.id) === at);
  render();
}

// An event shown on its tab's page, marked; the search cleared.
function showEvent(id) {
  if (!byId.has(id)) {
    return;
  }

  $('search').value = '';
  marked = id;
  shownTab = fileOf(id);
  tabs.forEach((tab, index) => tab.classList.toggle('chosen', index === shownTab));
  list = DATA.events.filter(event => fileOf(event.id) === shownTab);
  render(id);
  keepPlace(String(id));
}

function search() {
  const query = $('search').value.trim();
  tabs.forEach((tab, index) => tab.classList.toggle('chosen', index === shownTab && !query));

  if (!query) {
    showTab(shownTab);
    return;
  }

  list = found(query);
  render();
}

// The event shown kept in the address (and in the home page's, which opens it there), so that its link can be copied.
function keepPlace(detail) {
  history.replaceState(null, '', `${location.search}#${encodeURIComponent(detail)}`);

  if (window.parent !== window) {
    window.parent.postMessage({ shown: 'events', detail }, '*');
  }
}

function addressed() {
  try {
    return decodeURIComponent(location.hash.slice(1));
  } catch {
    return '';
  }
}

// The address names an event (events.html#5300) or a tab (#第三卷); the first tab otherwise.
function showAddressed() {
  const text = addressed();
  const id = Number(text);

  if (text && byId.has(id)) {
    showEvent(id);
    return;
  }

  marked = 0;
  showTab(Math.max(DATA.tabs.findIndex(tab => tab.label === text), 0));
}

// ←→ step through the tabs, wrapping round as the other pages' do; the search cleared.
function turn(step) {
  $('search').value = '';
  marked = 0;
  showTab((shownTab + step + tabs.length) % tabs.length);
  keepPlace(DATA.tabs[shownTab].label);
}

// The book keeps its size: scaled whole to fit the window, centred.
function fit() {
  const book = document.querySelector('.book');
  const scale = Math.min(innerWidth / book.offsetWidth, innerHeight / book.offsetHeight);
  const left = (innerWidth - book.offsetWidth * scale) / 2;
  const top = (innerHeight - book.offsetHeight * scale) / 2;
  book.style.transform = `translate(${left}px, ${top}px) scale(${scale})`;
}

$('tabs').addEventListener('click', event => {
  const tab = event.target.closest('.tab');

  if (tab) {
    $('search').value = '';
    marked = 0;
    showTab(Number(tab.dataset.at));
    keepPlace(DATA.tabs[shownTab].label);
  }
});

$('body').addEventListener('click', event => {
  const link = event.target.closest('a[data-event]');

  if (link) {
    showEvent(Number(link.dataset.event));
  }
});

$('search').addEventListener('input', search);

document.addEventListener('keydown', event => {
  if (event.target === $('search')) {
    return;
  }

  const body = $('body');
  const page = body.clientHeight - 40;
  const keys = {
    ArrowUp: () => { body.scrollTop -= 40; },
    ArrowDown: () => { body.scrollTop += 40; },
    PageUp: () => { body.scrollTop -= page; },
    PageDown: () => { body.scrollTop += page; },
    Home: () => { body.scrollTop = 0; },
    End: () => { body.scrollTop = body.scrollHeight; },
    ArrowLeft: () => turn(-1),
    ArrowRight: () => turn(1),
  };

  if (keys[event.key]) {
    keys[event.key]();
    event.preventDefault();
  }
});

addEventListener('resize', fit);
addEventListener('hashchange', showAddressed);
fit();
showAddressed();
</script>
</body>
</html>
'''

if __name__ == '__main__':
    main()
