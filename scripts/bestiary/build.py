"""scripts/bestiary/build.py <dump dir> <docs dir>: the swd3e bestiary page (docs/bestiary.html) and the game's
pictures it is drawn with (docs/ui).

The dump dir holds the output of dump: monsters.jsonl (data), portraits.raw (portrait 59 1 500), bestiary_bg.raw
(portrait 50 9002 9002), tabs.raw (pic 9030 0 10), tab.raw (pic 9058 0 10).

The rules the page states come from the rebuild's verified code: Menu::BestiaryEnter/List/Choose/Lines
(0x00441990..0x00442920), ApplyElement (the element rules of src/battle/battle_unit.cpp), BattleRole::Steal,
BattleUnit::RollDrop (0x00488D40), BattleUnit::CatchRoll (the enemy's flight), HitDamage's ailment roll
(0x00497900), BattleUnit::EnrageCheck (0x0049BB50) and Enrage (0x0049BE20), Game::RemoveDefeated's rewards.
"""

import base64
import io
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

# The game's pictures: frame 9002 of the interface's background actions, the bestiary's frames inside it.
BACKGROUND_ID = 9002
BOX_CROP = (230, 60, 502, 279)  # the frame of the bestiary's lines, cut out to draw the page's own boxes
TAB_FONT = '/mnt/c/Windows/Fonts/msjhbd.ttc'
TAB_TEXT = 12
TAB_TEXT_CHOSEN = 13
TAB_INK = (56, 64, 24, 255)
TAB_INK_CHOSEN = (24, 24, 0, 255)

# The tabs of the game's bestiary: the category values of 0x00442280 and their names at 0x004AF94C.
CATEGORIES = [
    (0, '天神'), (1, '魔神'), (2, '仙靈'), (3, '神獸'), (4, '魔獸'), (5, '妖靈'),
    (6, '精怪'), (7, '鬼魂'), (8, '活屍'), (10, '生物'), (18, '人族'),
]
# The rebuild's page only: the fought records the game's bestiary leaves out (story opponents of category 15).
OTHER_NAME = '其他'
OTHER_CATEGORIES = {15}
CARD_SIZE = (120, 220)  # a bestiary picture; other sizes are battle poses or story pictures
ELEMENTS = ['金', '木', '水', '火', '土']
# ApplyElement: for an attack of each element the resistance that lessens it, the one that raises it, the one that
# absorbs it (indices into the monster's five resistance bytes, signed percents).
ELEMENT_RULES = [(3, 1, 2), (0, 4, 3), (4, 3, 1), (2, 0, 4), (1, 2, 0)]
BOSS_KIND = 0x20

# EnrageCheck: what sets an enrage off (the conditions the data uses that the code makes clear).
ENRAGE_CONDITIONS = {
    1: '生命低於上限的三分之一時',
    2: '只剩自己一個敵人時',
    5: '受到會心一擊時',
    10: '受到金屬性攻擊時', 11: '受到木屬性攻擊時', 12: '受到水屬性攻擊時',
    13: '受到火屬性攻擊時', 14: '受到土屬性攻擊時',
    15: '收化失敗時',
    0x13: '逃跑失敗時',
    0x16: '我方撤退失敗時',
    0x17: '我方有角色倒下時',
    0x18: '我方角色復活時',
    0x19: '我方角色靈力不足時',
    0x1A: '我方角色體力不足時',
    0x1B: '閃過攻擊時',
    0x1C: '第二次施展回復後',
    0x1D: '同場有同種敵人時',
    0x1E: '法術或招式的次數用完時',
}
ENRAGE_OTHER_CONDITION = '特定情況下'
# Enrage: what it does (by effect value; below 1001 a new move).
ENRAGE_EFFECTS = {
    1001: '攻擊提高七成', 1002: '防禦提高七成', 1003: '敏捷提高七成',
    1004: '攻擊加倍、防禦減半', 1005: '攻擊加倍、敏捷減半', 1006: '防禦加倍、攻擊減半',
    1007: '防禦加倍、敏捷減半', 1008: '敏捷加倍、攻擊減半', 1009: '敏捷加倍、防禦減半',
    1010: '進入特殊狀態', 1011: '隱身',
    1012: '攻擊時另造成相當於自身生命上限 1.4 倍的傷害',
    1013: '專挑生命最少的角色攻擊', 1014: '專挑靈力最多的角色攻擊',
    1015: '專挑體力最多的角色攻擊', 1016: '專挑敏捷最低的角色攻擊',
    1017: '專挑防禦最低的角色攻擊', 1018: '專挑攻擊最高的角色攻擊',
}
ENRAGE_NEW_MOVE = '改用新的招式'


def tdiv(a, b):
    """C integer division (towards zero)."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


def element_taken(resistances):
    """The damage an attack of 100 of each element does (ApplyElement): [kind, text] per element."""
    out = []

    for lessens, raises, absorbs in ELEMENT_RULES:
        value = 100

        if resistances[lessens]:
            value -= tdiv(resistances[lessens] * value, 100)

        if resistances[raises]:
            value += tdiv(value * resistances[raises], 100)

        if resistances[absorbs]:
            value = tdiv(value * resistances[absorbs], -100)

        if value < 0:
            out.append(['absorb', f'吸收{-value}%'])
        elif value == 0:
            out.append(['strong', '無效'])
        else:
            out.append(['weak' if value > 100 else 'strong' if value < 100 else '', f'{value}%'])

    return out


def enrage_text(record):
    condition = record['enrageCondition']

    if condition == 0:
        return ''

    effect = record['enrageEffect']

    if 0 < effect < 1001:
        what = ENRAGE_NEW_MOVE
    else:
        what = ENRAGE_EFFECTS.get(effect, '')

    if not what:
        return ''

    return f"{ENRAGE_CONDITIONS.get(condition, ENRAGE_OTHER_CONDITION)}狂暴：{what}"


def dedupe(names):
    out = []

    for name in names:
        out.append(name if name and name not in out else '')

    return out


def read_pictures(path):
    """The pictures of a dump stream ("id width height" and RGBA rows) as PIL images by id."""
    pictures = {}

    with open(path, 'rb') as raw:
        while True:
            header = raw.readline()

            if not header:
                break

            ident, width, height = (int(part) for part in header.split())
            pictures[ident] = Image.frombytes('RGBA', (width, height), raw.read(width * height * 4))

    return pictures


def png(image):
    out = io.BytesIO()
    image.save(out, 'PNG', optimize=True)
    return out.getvalue()


def data_uri(image):
    return 'data:image/png;base64,' + base64.b64encode(png(image)).decode('ascii')


def other_tab(base, size):
    """A tab of the rebuild's 「其他」 in the look of the game's: the tab of the last category with its letters
    covered by the tab's own fill and the two new ones written over it."""
    tab = base.copy()
    width, height = tab.size
    pixels = tab.load()
    # The fill of the tab at each row, from a column left of the letters.
    for y in range(height):
        fill = pixels[5, y]

        for x in range(6, width - 6):
            if pixels[x, y][3] and pixels[x, y] != fill and abs(sum(pixels[x, y][:3]) - sum(fill[:3])) > 60:
                pixels[x, y] = fill

    draw = ImageDraw.Draw(tab)
    font = ImageFont.truetype(TAB_FONT, size)
    ink = TAB_INK_CHOSEN if size == TAB_TEXT_CHOSEN else TAB_INK
    draw.fontmode = '1'
    draw.text((width // 2, height // 2 + 1), OTHER_NAME, font=font, fill=ink, anchor='mm')
    return tab


def write_ui(ui_path, docs):
    """The pictures of the game the pages are drawn with, into docs/ui: the bestiary's background with its frames,
    one frame to draw boxes with, the tabs of the categories."""
    out = os.path.join(docs, 'ui')
    os.makedirs(out, exist_ok=True)
    bestiary = read_pictures(os.path.join(ui_path, 'bestiary_bg.raw'))[BACKGROUND_ID]
    tabs = read_pictures(os.path.join(ui_path, 'tabs.raw'))
    chosen = read_pictures(os.path.join(ui_path, 'tab.raw'))
    tabs[len(CATEGORIES)] = other_tab(tabs[len(CATEGORIES) - 1], TAB_TEXT)
    chosen[len(CATEGORIES)] = other_tab(chosen[len(CATEGORIES) - 1], TAB_TEXT_CHOSEN)
    files = {
        'bestiary.png': bestiary.convert('RGB'),
        'box.png': bestiary.crop(BOX_CROP).convert('RGB'),
    }

    for at in range(len(CATEGORIES) + 1):
        files[f'tab{at}.png'] = tabs[at]
        files[f'chosen{at}.png'] = chosen[at]

    for name, image in files.items():
        with open(os.path.join(out, name), 'wb') as file:
            file.write(png(image))


def main():
    dump, docs = sys.argv[1:3]
    portraits = read_pictures(os.path.join(dump, 'portraits.raw'))
    write_ui(dump, docs)
    tabs = {value: at for at, (value, _) in enumerate(CATEGORIES)}
    monsters = []

    with open(os.path.join(dump, 'monsters.jsonl'), encoding='utf-8') as lines:
        records = [json.loads(line) for line in lines if line.strip()]

    for r in records:
        picture = portraits.get(r['id'])
        size = picture.size if picture else None

        if r['category'] in tabs:
            tab = tabs[r['category']]
        elif r['category'] in OTHER_CATEGORIES and r['hp'] > 0 and size == CARD_SIZE:
            tab = len(CATEGORIES)
        else:
            continue

        # BestiaryLines: the three objects, each named once.
        items = dedupe(r['items'])
        monsters.append({
            'id': r['id'],
            'name': r['name'],
            'tab': tab,
            'level': r['level'],
            'hp': r['hp'],
            'spirit': r['spirit'],
            'stamina': r['stamina'],
            'attack': r['attack'],
            'defense': r['defense'],
            'agility': r['agility'],
            'items': items,
            'description': r['description'],
            'portrait': data_uri(picture) if picture else '',
            'hidden': {
                'experience': r['experience'],
                'gold': r['gold'],
                'boss': 1 if r['kind'] & BOSS_KIND else 0,
                'wisdom': r['wisdom'],
                'elements': element_taken(r['resistances']),
                'ailmentChance': r['ailmentChance'],
                'fleeChance': r['fleeChance'],
                'steal': r['steal'],
                'stealDifficulty': r['stealDifficulty'],
                'drop': r['drop'],
                'dropChance': r['dropChance'],
                'itemCounts': [count if name else 0 for name, count in zip(items, r['itemCounts'])],
                'heals': r['heals'],
                'healCounts': r['healCounts'],
                'enrage': enrage_text(r),
            },
        })

    data = {
        'categories': [{'name': name} for _, name in CATEGORIES] + [{'name': OTHER_NAME}],
        'monsters': monsters,
    }
    page = TEMPLATE.replace('/*DATA*/null', json.dumps(data, ensure_ascii=False, separators=(',', ':')))
    page = page.replace('/*ELEMENTS*/null', json.dumps(ELEMENTS, ensure_ascii=False))

    with open(os.path.join(docs, 'bestiary.html'), 'w', encoding='utf-8') as out:
        out.write(page)

    print(f'{len(monsters)} monsters, {sum(1 for m in monsters if m["portrait"])} portraits, {len(page)} bytes')


# The page: the game's bestiary screen (640×480, its background with the frames) at the coordinates of
# Menu::BestiaryDraw and DrawBestiaryRow, all values shown, and beside it the values the game does not show, in boxes
# drawn with the game's frame.
TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>神魔異事錄 — 軒轅劍參外傳 天之痕</title>
<style>
:root {
  --text: rgb(200, 184, 136);
  --shadow: rgb(48, 32, 24);
  --dim: rgb(150, 136, 98);
  --gold: rgb(240, 216, 120);
  --weak: #ff9070;
  --resist: #8fb4ff;
  --absorb: #7fe0a0;
  --ming: "MingLiU", "PMingLiU", "細明體", "新細明體", "SimSun", serif;
}

* { box-sizing: border-box; }

html, body { margin: 0; height: 100%; overflow: hidden; background: #1a0d04; }

body {
  font-family: var(--ming);
  color: var(--text);
  background: radial-gradient(ellipse at center, #3a2410 0%, #120802 75%);
}

.book {
  position: absolute;
  left: 0;
  top: 0;
  width: 960px;
  height: 480px;
  transform-origin: 0 0;
}

.screen {
  position: absolute;
  left: 0;
  top: 0;
  width: 640px;
  height: 480px;
  background: url(ui/bestiary.png);
  box-shadow: 0 0 12px rgba(0, 0, 0, 0.8);
  image-rendering: pixelated;
}

.screen > *, .side > * { position: absolute; }

.shadowed { text-shadow: 1px 1px 0 var(--shadow); }

.tabs { left: 10px; top: 1px; width: 196px; height: 77px; overflow: hidden; }
.tabs img { position: absolute; cursor: pointer; image-rendering: pixelated; }

.list { left: 10px; top: 88px; width: 196px; height: 374px; overflow: hidden; }
.row {
  position: absolute;
  left: 4px;
  width: 189px;
  height: 24px;
  cursor: pointer;
  white-space: nowrap;
}
.row .name { position: absolute; left: 4px; top: 2px; font-size: 20px; line-height: 20px; }
.row .level { position: absolute; left: 154px; top: 11px; font-size: 12px; line-height: 12px; }
.row.chosen::after {
  content: "";
  position: absolute;
  inset: 0;
  background: linear-gradient(to bottom, rgba(160, 104, 0, 0) 0%, rgba(160, 104, 0, 0.9) 50%, rgba(160, 104, 0, 0) 100%);
  mix-blend-mode: screen;
  pointer-events: none;
}
.empty { position: absolute; left: 8px; top: 8px; font-size: 20px; }

.bar { left: 200px; top: 98px; width: 5px; height: 350px; }
.bar div { position: absolute; left: 0; width: 5px; background: rgb(160, 136, 72); border: 1px solid rgb(64, 40, 16); }

.lines { left: 246px; top: 72px; width: 250px; font-size: 16px; line-height: 20px; white-space: pre; }
.boss { font-size: 12px; color: var(--weak); border: 1px solid var(--weak); padding: 0 2px; margin-left: 6px; line-height: 14px; vertical-align: 2px; }

.portrait { left: 508px; top: 60px; image-rendering: pixelated; }

.items { left: 246px; top: 294px; width: 376px; height: 18px; font-size: 16px; line-height: 16px; white-space: nowrap; }
.items span { position: absolute; top: 0; }

.description {
  left: 242px;
  top: 336px;
  width: 372px;
  height: 125px;
  font-size: 20px;
  line-height: 25px;
  overflow: hidden;
}

.side { position: absolute; left: 656px; top: 0; width: 304px; height: 480px; }

.box {
  border: 8px solid transparent;
  border-image: url(ui/box.png) 8 fill / 8px stretch;
  image-rendering: pixelated;
  padding: 2px 8px;
}

.search-box { left: 0; top: 14px; width: 304px; height: 36px; padding: 0 6px; }
.search {
  width: 100%;
  height: 100%;
  border: none;
  outline: none;
  background: transparent;
  font: 16px var(--ming);
  color: var(--text);
}
.search::placeholder { color: var(--dim); }

.info { left: 0; top: 56px; width: 304px; height: 126px; font-size: 16px; line-height: 21px; }
.info table, .loot table { border-collapse: collapse; width: 100%; }
.info td { padding: 0; white-space: nowrap; }
.info td.value { text-align: right; }

.resist { left: 0; top: 188px; width: 304px; height: 60px; display: grid; grid-template-columns: 52px repeat(5, 1fr);
  align-items: center; text-align: center; font-size: 16px; }
.resist .label { text-align: left; color: var(--dim); }
.resist .value { font-size: 12px; line-height: 14px; color: var(--dim); }
.resist .weak .value { color: var(--weak); }
.resist .strong .value { color: var(--resist); }
.resist .absorb .value { color: var(--absorb); }

.loot { left: 0; top: 254px; width: 304px; height: 188px; font-size: 13px; line-height: 17px; overflow-y: auto;
  scrollbar-width: thin; scrollbar-color: rgb(160, 136, 72) rgb(16, 16, 8); }
.loot td { vertical-align: top; padding: 1px 0; }
.loot td.label { width: 38px; color: var(--dim); white-space: nowrap; }
.note { color: var(--dim); font-size: 12px; }

.hint { left: 0; top: 448px; width: 304px; font-size: 11px; line-height: 13px; color: rgb(255, 236, 190);
  text-shadow: 1px 1px 0 #000; }
</style>
</head>
<body>
<div class="book" id="book">
  <div class="screen">
    <div class="tabs" id="tabs"></div>
    <div class="list shadowed" id="list"></div>
    <div class="bar" id="bar"></div>
    <div class="lines shadowed" id="lines"></div>
    <img class="portrait" id="portrait" alt="">
    <div class="items shadowed" id="items"></div>
    <div class="description shadowed" id="description"></div>
  </div>
  <div class="side">
    <div class="box search-box"><input class="search" id="search" type="search" placeholder="搜尋（空格分隔多個關鍵字）" autocomplete="off"></div>
    <div class="box info shadowed" id="info"></div>
    <div class="box resist shadowed" id="resist"></div>
    <div class="box loot shadowed" id="loot"></div>
    <div class="hint">↑↓ 選擇　←→ 分類　PgUp / PgDn 翻頁　Esc 清除搜尋<br>「其他」為遊戲圖鑑未收錄的對手</div>
  </div>
</div>
<script>
const DATA = /*DATA*/null;
const ELEMENTS = /*ELEMENTS*/null;
const ROWS = 15;
const ROW_HEIGHT = 24;
const tabName = m => DATA.categories[m.tab].name;
let category = 0;
let chosen = 0;
let first = 0;
let shown = [];

const $ = id => document.getElementById(id);

function escape(text) {
  return text.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
}

function percent(value, most) {
  return Math.min(value, most) * (100 / most);
}

// Everything a monster shows as text, for the search.
function searchText(m) {
  const h = m.hidden;
  return [m.name, tabName(m), `${m.level}級`, m.description, ...m.items, ...h.heals, h.steal, h.drop, h.enrage,
    h.boss ? 'BOSS' : ''].join('\n').toLowerCase();
}

DATA.monsters.forEach(m => { m.text = searchText(m); });

function query() {
  return $('search').value.trim().toLowerCase().split(/\s+/).filter(word => word);
}

// DrawBestiaryTabs: the two tabs on each side of the chosen one, then the chosen one over them.
function drawTabs(at) {
  const html = [];

  for (let tab = at - 2; tab <= at + 2; ++tab) {
    if (tab >= 0 && tab < DATA.categories.length && tab !== at) {
      html.push(`<img src="ui/tab${tab}.png" style="left:${6 + 40 * (tab - at + 2) - 10}px;top:60px" data-at="${tab}" alt="${DATA.categories[tab].name}">`);
    }
  }

  html.push(`<img src="ui/chosen${at}.png" style="left:76px;top:57px" data-at="${at}" alt="${DATA.categories[at].name}">`);
  $('tabs').innerHTML = html.join('');
}

function list() {
  const words = query();
  shown = words.length ? DATA.monsters.filter(m => words.every(word => m.text.includes(word))) :
    DATA.monsters.filter(m => m.tab === category);
  chosen = Math.min(chosen, Math.max(shown.length - 1, 0));
  show();
}

function drawList() {
  first = Math.max(0, Math.min(first, Math.max(shown.length - ROWS, 0)));

  if (chosen < first) {
    first = chosen;
  } else if (chosen >= first + ROWS) {
    first = chosen - ROWS + 1;
  }

  const rows = shown.slice(first, first + ROWS).map((m, row) =>
    `<div class="row${first + row === chosen ? ' chosen' : ''}" style="top:${6 + ROW_HEIGHT * row}px" data-at="${first + row}">` +
    `<span class="name">${escape(m.name)}</span><span class="level">${m.level}級</span></div>`);
  $('list').innerHTML = rows.join('') || '<div class="empty">找不到</div>';

  if (shown.length > ROWS) {
    const height = Math.max(12, 350 * ROWS / shown.length);
    const at = (350 - height) * first / (shown.length - ROWS);
    $('bar').innerHTML = `<div style="top:${at}px;height:${height}px"></div>`;
  } else {
    $('bar').innerHTML = '';
  }
}

// BattleUnit::CatchRoll: an enemy flees only from a party led by a role 5 levels above it or more.
function flee(chance) {
  if (chance === 0) {
    return '不會逃跑';
  }

  return `隊首高 5 級 ${percent(chance, 10)}%，高 6～10 級 50%，高 11 級以上 80%；偷走東西後 80%`;
}

// HitDamage: an ailment lands on a roll of 0..9 below the monster's chance plus a tenth for each 20 points of the
// caster's wisdom above the monster's.
function ailment(h) {
  return h.ailmentChance === 0 ? '不會中異常' : `${percent(h.ailmentChance, 10)}%`;
}

function counted(name, count) {
  return `${escape(name)}<span class="note">×${count}</span>`;
}

const pad = value => String(value).padStart(4, ' ');

function show() {
  drawList();
  const m = shown[chosen];
  drawTabs(m && query().length ? m.tab : category);

  if (!m) {
    ['lines', 'items', 'description', 'info', 'resist', 'loot'].forEach(id => { $(id).innerHTML = ''; });
    $('portrait').removeAttribute('src');
    return;
  }

  const h = m.hidden;
  // BestiaryLines: the category, the level, the name, then the six values as the game shows them once known.
  const values = [['生命', m.hp], ['靈力', m.spirit], ['體力', m.stamina], ['攻擊', m.attack], ['防禦', m.defense],
    ['敏捷', m.agility]];
  $('lines').innerHTML = [escape(tabName(m)), `${pad(m.level)}級`,
    escape(m.name) + (h.boss ? '<span class="boss">BOSS</span>' : ''),
    ...values.map(([label, value]) => `${label} 是 ${pad(value)}`)].join('\n');
  $('portrait').src = m.portrait;
  $('portrait').alt = m.name;
  $('items').innerHTML = m.items.filter(item => item).map((item, at) =>
    `<span style="left:${126 * at}px">${escape(item)}</span>`).join('');
  $('description').textContent = m.description;

  const side = [['經驗值', h.experience], ['銀兩', h.gold], ['法寶經驗', h.boss ? 20 : 3], ['智慧', h.wisdom],
    ['<span title="施術者智慧每比怪物高 20 點加 10%">中異常機率</span>', ailment(h)]];
  $('info').innerHTML = `<table>${side.map(([label, value]) =>
    `<tr><td>${label}</td><td class="value">${value}</td></tr>`).join('')}</table>`;
  $('resist').innerHTML = '<div class="label">承受<br>傷害</div>' + ELEMENTS.map((element, at) => {
    const [kind, text] = h.elements[at];
    return `<div class="${kind}"><div>${element}</div><div class="value">${text}</div></div>`;
  }).join('');
  const moves = m.items.map((item, at) => item ? counted(item, h.itemCounts[at]) : '').filter(item => item);
  const heals = h.heals.map((item, at) => item ? counted(item, h.healCounts[at]) : '').filter(item => item);
  const steal = h.steal ?
    `${escape(h.steal)}<br><span class="note">成功率 ${percent(h.stealDifficulty, 10)}%，偷竊飾品熟練度每 25 加 10%，滿 100 必成</span>` :
    '無';
  const drop = h.drop ? `${escape(h.drop)}<span class="note">　${percent(h.dropChance, 20)}%</span>` : '無';
  const lines = [['法術', moves.join('　') || '無'], ['回復', heals.join('　') || '無'], ['偷竊', steal],
    ['掉落', drop], ['逃跑', `<span class="note">${flee(h.fleeChance)}</span>`],
    ['狂暴', h.enrage ? escape(h.enrage) : '無']];
  $('loot').innerHTML = `<table>${lines.map(([label, text]) =>
    `<tr><td class="label">${label}</td><td>${text}</td></tr>`).join('')}</table>`;
}

function move(by) {
  if (!shown.length) {
    return;
  }

  chosen = Math.max(0, Math.min(shown.length - 1, chosen + by));
  show();
}

function turn(to) {
  category = (to + DATA.categories.length) % DATA.categories.length;
  chosen = 0;
  first = 0;
  $('search').value = '';
  list();
}

// While searching the list holds the monsters of every category that match all the words.
$('search').addEventListener('input', () => {
  chosen = 0;
  first = 0;
  list();
});

$('tabs').addEventListener('click', event => {
  const tab = event.target.closest('img');

  if (tab) {
    turn(Number(tab.dataset.at));
  }
});

$('list').addEventListener('click', event => {
  const row = event.target.closest('.row');

  if (row) {
    chosen = Number(row.dataset.at);
    show();
  }
});

$('list').addEventListener('wheel', event => {
  first += event.deltaY > 0 ? 3 : -3;
  first = Math.max(0, Math.min(first, Math.max(shown.length - ROWS, 0)));
  chosen = Math.max(first, Math.min(chosen, first + ROWS - 1));
  show();
  event.preventDefault();
}, { passive: false });

document.addEventListener('keydown', event => {
  const typing = event.target === $('search');

  if (typing && event.key === 'Escape') {
    $('search').value = '';
    $('search').dispatchEvent(new Event('input'));
    return;
  }

  if (typing && (event.key === 'ArrowLeft' || event.key === 'ArrowRight')) {
    return;
  }

  const keys = { ArrowUp: () => move(-1), ArrowDown: () => move(1), PageUp: () => move(-ROWS),
    PageDown: () => move(ROWS), ArrowLeft: () => turn(category - 1), ArrowRight: () => turn(category + 1) };

  if (keys[event.key]) {
    keys[event.key]();
    event.preventDefault();
  }
});

// The book keeps its size: scaled whole to fit the window, centred.
function fit() {
  const book = $('book');
  const scale = Math.min(innerWidth / book.offsetWidth, innerHeight / book.offsetHeight) * 0.96;
  const left = (innerWidth - book.offsetWidth * scale) / 2;
  const y = (innerHeight - book.offsetHeight * scale) / 2;
  book.style.transform = `translate(${left}px, ${y}px) scale(${scale})`;
}

addEventListener('resize', fit);
fit();
list();
</script>
</body>
</html>
'''

if __name__ == '__main__':
    main()
