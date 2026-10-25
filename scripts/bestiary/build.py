"""scripts/bestiary/build.py <monsters.jsonl> <portraits.raw> <out.html>: the swd3e bestiary as one HTML page.

The rules the page states come from the rebuild's verified code: Menu::BestiaryEnter/List/Choose/Lines
(0x00441990..0x00442920), ApplyElement (the element rules of src/battle/battle_unit.cpp), BattleRole::Steal,
BattleUnit::RollDrop (0x00488D40), BattleUnit::CatchRoll (the enemy's flight), HitDamage's ailment roll
(0x00497900), BattleUnit::EnrageCheck (0x0049BB50) and Enrage (0x0049BE20), Game::RemoveDefeated's rewards.
"""

import base64
import io
import json
import sys

from PIL import Image

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


def read_portraits(path):
    portraits = {}

    with open(path, 'rb') as raw:
        while True:
            header = raw.readline()

            if not header:
                break

            ident, width, height = (int(part) for part in header.split())
            pixels = raw.read(width * height * 3)
            image = Image.frombytes('RGB', (width, height), pixels)
            out = io.BytesIO()
            image.save(out, 'PNG', optimize=True)
            portraits[ident] = ('data:image/png;base64,' + base64.b64encode(out.getvalue()).decode('ascii'), (width, height))

    return portraits


def main():
    monsters_path, portraits_path, out_path = sys.argv[1:4]
    portraits = read_portraits(portraits_path)
    tabs = {value: at for at, (value, _) in enumerate(CATEGORIES)}
    monsters = []

    with open(monsters_path, encoding='utf-8') as lines:
        records = [json.loads(line) for line in lines if line.strip()]

    for r in records:
        picture, size = portraits.get(r['id'], ('', None))

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
            'portrait': picture,
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

    with open(out_path, 'w', encoding='utf-8') as out:
        out.write(page)

    print(f'{len(monsters)} monsters, {sum(1 for m in monsters if m["portrait"])} portraits, {len(page)} bytes')


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>神魔異事錄</title>
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
  --weak: #ff8a6a;
  --resist: #8fb4ff;
  --absorb: #7fe0a0;
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

.search {
  width: 360px;
  height: 34px;
  margin: 0 0 6px 6px;
  padding: 0 10px;
  font: inherit;
  font-size: 18px;
  color: var(--text);
  background: var(--panel);
  border: 2px solid var(--gold);
  box-shadow: 0 0 0 1px #000, inset 0 0 0 1px #000;
  outline: none;
}

.search::placeholder { color: var(--dim); }
.search:focus { border-color: var(--gold-light); }

.empty { font-family: var(--kai); font-size: 20px; color: var(--dim); padding: 8px 6px; }

.layout {
  display: grid;
  grid-template-columns: 96px 280px 508px 230px;
  grid-template-rows: 300px 64px 202px 1fr;
  gap: 10px 14px;
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
  box-shadow:
    inset 0 0 0 1px #000,
    0 0 0 1px #000,
    0 0 0 3px rgba(160, 136, 72, 0.55);
  position: relative;
}

.tabs {
  grid-column: 1;
  grid-row: 1 / span 4;
  display: flex;
  flex-direction: column;
  gap: 3px;
  padding-top: 10px;
  margin-right: -11px;
  overflow-y: auto;
  scrollbar-width: none;
  z-index: 1;
}

.tab {
  font-family: var(--kai);
  font-size: 19px;
  padding: 5px 12px 5px 10px;
  color: #2c3410;
  background: linear-gradient(90deg, #5c7034, #889c58);
  border: 2px solid #b89838;
  border-right: none;
  border-radius: 10px 0 0 10px;
  box-shadow: -1px 0 0 #3a2a08, 0 -1px 0 #3a2a08, 0 1px 0 #3a2a08;
  cursor: pointer;
  user-select: none;
  text-align: center;
  white-space: nowrap;
}

.tab.chosen {
  color: #181800;
  background: linear-gradient(90deg, #b8d050, #eef488);
  border-color: #e0c048;
  font-weight: bold;
}

.tab.found {
  color: #20280c;
  background: linear-gradient(90deg, #7c9444, #a8bc68);
  border-color: #d0b040;
  box-shadow: -1px 0 0 #3a2a08, 0 -1px 0 #3a2a08, 0 1px 0 #3a2a08, inset 4px 0 0 #e0c048;
}

.list {
  grid-column: 2;
  grid-row: 1 / span 4;
  min-height: 0;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable;
  padding: 12px 6px 12px 10px;
  --fill: var(--panel-list);
  scrollbar-color: var(--gold) #000;
}

.row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  font-family: var(--kai);
  font-size: 25px;
  line-height: 40px;
  padding: 0 10px 0 6px;
  cursor: pointer;
  white-space: nowrap;
  text-shadow: 1px 1px 0 #302018;
}

.row .level { font-family: var(--ming); font-size: 15px; color: var(--dim); }

/* The game's highlight: a band brightest in the middle, fading to the top and bottom edges. */
.row.chosen {
  color: #fff0b0;
  background: linear-gradient(180deg, rgba(160, 104, 0, 0), rgba(200, 130, 10, 0.95) 50%, rgba(160, 104, 0, 0));
}

.row.chosen .level { color: #fff0c0; }

.stats {
  grid-column: 3;
  grid-row: 1;
  display: grid;
  grid-template-columns: 200px 1fr;
  column-gap: 32px;
  padding: 12px 24px;
  font-size: 20px;
  line-height: 31px;
  overflow: hidden;
}

.stats .kind { font-family: var(--kai); font-size: 24px; }
.stats .main, .stats .side { min-width: 0; }
.stats .name {
  font-family: var(--kai);
  color: #f0e0a8;
  font-size: 30px;
  height: 40px;
  line-height: 40px;
  margin-bottom: 4px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.stats .boss { vertical-align: middle; line-height: 18px; font-size: 15px; color: var(--weak); border: 1px solid var(--weak); padding: 0 4px; margin-left: 6px; }

.stats table { border-collapse: collapse; table-layout: fixed; }
.stats td { padding: 0; white-space: nowrap; }
.stats td.value { text-align: right; font-variant-numeric: tabular-nums; }
.stats .main table { width: 140px; }
.stats .main td:first-child { width: 60px; }
.stats .side table { width: 210px; }
.stats .side td:first-child { width: 130px; }
.stats .side { padding-top: 70px; }
.stats .side td:first-child { color: var(--dim); }

.portrait {
  grid-column: 4;
  grid-row: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 10px;
  background: linear-gradient(135deg, #6a6020, #3c3410);
  border: 3px solid #a89048;
  box-shadow: inset 0 0 0 2px #1c1806, 0 0 0 1px #000;
}

.portrait img {
  max-width: 100%;
  max-height: 270px;
  border: 2px solid #1c1806;
  background: #f4ecd8;
}

.resist {
  grid-column: 3 / span 2;
  grid-row: 2;
  display: grid;
  grid-template-columns: 104px repeat(5, minmax(0, 1fr));
  align-items: center;
  padding: 6px 24px;
  font-size: 19px;
  text-align: center;
}

.resist .label, .loot .label { font-family: var(--kai); color: var(--dim); text-align: left; padding-right: 14px; }
.resist .element { font-family: var(--kai); font-size: 22px; }
.resist > div { height: 50px; }
.resist .label { line-height: 50px; }
.resist .element { height: 28px; line-height: 28px; }
.resist .value { height: 20px; line-height: 20px; white-space: nowrap; font-size: 15px; color: var(--dim); font-variant-numeric: tabular-nums; }
.resist .weak .value { color: var(--weak); }
.resist .strong .value { color: var(--resist); }
.resist .absorb .value { color: var(--absorb); }

.loot {
  grid-column: 3 / span 2;
  grid-row: 3;
  display: grid;
  grid-template-columns: 104px minmax(0, 1fr);
  align-content: start;
  grid-auto-rows: 30px;
  padding: 8px 24px;
  font-size: 18px;
  line-height: 28px;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable;
  scrollbar-color: var(--gold) #000;
}

.loot .note { color: var(--dim); font-size: 15px; }
.loot > div { line-height: 30px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }

.description {
  grid-column: 3 / span 2;
  grid-row: 4;
  padding: 12px 24px;
  font-size: 23px;
  line-height: 38px;
  min-height: 0;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable;
  letter-spacing: 1px;
  scrollbar-color: var(--gold) #000;
}

.hint { flex: none; margin-top: 6px; font-size: 13px; color: #3a2008; text-align: right; }

</style>
</head>
<body>
<div class="book">
  <div class="top">
    <input class="search" id="search" type="search" placeholder="搜尋名稱、法術、物品、說明" autocomplete="off">
    <h1>神魔異事錄</h1>
  </div>
  <div class="layout">
    <div class="tabs" id="tabs"></div>
    <div class="frame list" id="list"></div>
    <div class="frame stats" id="stats"></div>
    <div class="portrait" id="portrait"></div>
    <div class="frame resist" id="resist"></div>
    <div class="frame loot" id="loot"></div>
    <div class="frame description" id="description"></div>
  </div>
  <div class="hint">↑↓ 選擇　←→ 切換分類　PageUp / PageDown 翻頁　搜尋可空格分隔多個關鍵字，Esc 清除　「其他」為遊戲圖鑑未收錄的對手　中異常機率：施術者智慧每比怪物高 20 點加 10%</div>
</div>
<script>
const DATA = /*DATA*/null;
const ELEMENTS = /*ELEMENTS*/null;
const tabName = m => DATA.categories[m.tab].name;
let category = 0;
let chosen = 0;
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

function drawTabs() {
  $('tabs').innerHTML = DATA.categories.map((c, at) =>
    `<div class="tab${at === category ? ' chosen' : ''}" data-at="${at}">${c.name}</div>`).join('');
}

function list() {
  const words = query();
  shown = words.length ? DATA.monsters.filter(m => words.every(word => m.text.includes(word))) :
    DATA.monsters.filter(m => m.tab === category);
  chosen = Math.min(chosen, Math.max(shown.length - 1, 0));
  $('list').innerHTML = shown.length ? shown.map((m, at) =>
    `<div class="row${at === chosen ? ' chosen' : ''}" data-at="${at}"><span>${escape(m.name)}</span>` +
    `<span class="level">${m.level}級</span></div>`).join('') : '<div class="empty">找不到</div>';
  show();
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
  if (h.ailmentChance === 0) {
    return '不會中異常';
  }

  return `${percent(h.ailmentChance, 10)}%`;
}

function counted(name, count) {
  return `${escape(name)}<span class="note"> ×${count}</span>`;
}

function show() {
  document.querySelectorAll('.row').forEach((row, at) => row.classList.toggle('chosen', at === chosen));
  const row = document.querySelector('.row.chosen');

  if (row) {
    row.scrollIntoView({ block: 'nearest' });
  }

  const m = shown[chosen];
  // While searching the tab of the monster chosen is marked (found), otherwise the category chosen.
  const searching = query().length > 0;
  document.querySelectorAll('.tab').forEach((tab, at) => {
    tab.classList.toggle('chosen', !searching && at === category);
    tab.classList.toggle('found', searching && m !== undefined && at === m.tab);
  });

  if (!m) {
    ['stats', 'portrait', 'resist', 'loot', 'description'].forEach(id => { $(id).innerHTML = ''; });
    return;
  }

  const h = m.hidden;
  const values = [['生命', m.hp], ['靈力', m.spirit], ['體力', m.stamina], ['攻擊', m.attack], ['防禦', m.defense],
    ['敏捷', m.agility]];
  const side = [['經驗值', h.experience], ['銀兩', h.gold], ['法寶經驗', h.boss ? 20 : 3],
    ['智慧', h.wisdom], ['中異常機率', ailment(h)]];
  const table = rows => `<table>${rows.map(([label, value]) =>
    `<tr><td>${label}</td><td class="value">${value}</td></tr>`).join('')}</table>`;
  $('stats').innerHTML =
    `<div class="main"><div class="kind">${tabName(m)}　${m.level}級</div>` +
    `<div class="name">${escape(m.name)}${h.boss ? '<span class="boss">BOSS</span>' : ''}</div>${table(values)}</div>` +
    `<div class="side">${table(side)}</div>`;
  $('portrait').innerHTML = m.portrait ? `<img src="${m.portrait}" alt="${escape(m.name)}">` : '';
  $('resist').innerHTML = '<div class="label">承受傷害</div>' + ELEMENTS.map((element, at) => {
    const [kind, text] = h.elements[at];
    return `<div class="${kind}"><div class="element">${element}</div><div class="value">${text}</div></div>`;
  }).join('');
  const moves = m.items.map((item, at) => item ? counted(item, h.itemCounts[at]) : '').filter(item => item);
  const heals = h.heals.map((item, at) => item ? counted(item, h.healCounts[at]) : '').filter(item => item);
  const steal = h.steal ?
    `${escape(h.steal)}<span class="note">　成功率 ${percent(h.stealDifficulty, 10)}%，偷竊饰品熟練度每 25 加 10%，滿 100 必成</span>` :
    '無';
  const drop = h.drop ? `${escape(h.drop)}<span class="note">　${percent(h.dropChance, 20)}%</span>` : '無';
  const lines = [['法術', moves.join('　') || '無'], ['回復', heals.join('　') || '無'], ['偷竊', steal],
    ['掉落', drop], ['逃跑', `<span class="note">${flee(h.fleeChance)}</span>`],
    ['狂暴', h.enrage ? escape(h.enrage) : '無']];
  $('loot').innerHTML = lines.map(([label, text]) => `<div class="label">${label}</div><div>${text}</div>`).join('');
  $('description').textContent = m.description;
}

function move(by) {
  if (!shown.length) {
    return;
  }

  chosen = Math.max(0, Math.min(shown.length - 1, chosen + by));
  show();
}

function turn(by) {
  category = (category + by + DATA.categories.length) % DATA.categories.length;
  chosen = 0;
  $('search').value = '';
  drawTabs();
  list();
}

// While searching the list holds the monsters of every category that match all the words.
$('search').addEventListener('input', () => {
  chosen = 0;
  list();
});

$('tabs').addEventListener('click', event => {
  const tab = event.target.closest('.tab');

  if (tab) {
    turn(Number(tab.dataset.at) - category);
  }
});

$('list').addEventListener('click', event => {
  const row = event.target.closest('.row');

  if (row) {
    chosen = Number(row.dataset.at);
    show();
  }
});

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

  const keys = { ArrowUp: () => move(-1), ArrowDown: () => move(1), PageUp: () => move(-15), PageDown: () => move(15),
    ArrowLeft: () => turn(-1), ArrowRight: () => turn(1) };

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

addEventListener('resize', fit);
fit();
drawTabs();
list();
</script>
</body>
</html>
'''

if __name__ == '__main__':
    main()
