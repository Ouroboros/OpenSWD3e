"""scripts/altar/build.py <objects.jsonl> <monsters.jsonl> <maps.dat> <pictures.raw> <out.html> [move.wav]: the swd3e altar (煉妖)
results and how to make each, as one HTML page.

The results follow Menu::AltarCombine (0x00444630) of the rebuild: a recipe of maps.dat table 0x18 (from +0x200, three
words: the two objects either way round and the result) gives its result; otherwise the matrix at the table's start
(16 a row; category 0x15 is looked up as 0xF) gives a category for (slot A, slot B), and among the objects 0x65..0x1F4
and 0xBB9..0xD47 of it the result is the first nearest the average level plus three, at it or above, or at it or below
when the half sum of the two categories is odd; a monster of 0x65..0x1F4 needs kind 0x800, an object of altar 1 never
results; with none 0x12A. The materials are what the altar's tabs list (AltarTabTakes, 0x00444950). A result more than
six levels above the first party role is refused (AltarSelect). The objects are those of scripts/items/dump (the items'
records 501..899 are the ids 0xBB9..0xD47, Database::LoadObject), the monsters' values those of scripts/bestiary/dump.
"""

import base64
import io
import json
import struct
import sys

from PIL import Image

ITEM_ID_SHIFT = 0x9C4
FIRST_ITEM_RECORD = 501
MAPS_HEADER = 0x200
RECIPES_TABLE = 0x18
RECIPES_AT = 0x200
MATRIX_SIDE = 0x10
DEFAULT = 0x12A
# The categories (Menu::BestiaryLines' names, 0x004AF94C) and the materials' (AltarTabTakes): 0..0xE and 0x15.
CATEGORY_NAMES = ['天神', '魔神', '仙靈', '神獸', '魔獸', '妖靈', '精怪', '鬼魂', '活屍', '晶礦', '生物', '武器', '防具',
                  '靈藥', '符器', '劇情', '法寶', '奇術', '人族', '不能練化', '絕技', '奇物']
TAB_KINDS = [(0x101F, False), (0x700, True), (0x800, True)]  # tested on the whole kind, or under KIND_MASK
KIND_MASK = 0x0FFF7FFF
LAST_TAB_CATEGORY = 0xE
EXTRA_TAB_CATEGORY = 0x15
LAST_MONSTER = 0x1F4
FIRST_LATE = 0xBB9
LAST_LATE = 0xD47


def game_id(record):
    return record + ITEM_ID_SHIFT if record >= FIRST_ITEM_RECORD else record


def tab_of(item):
    category = item['category']
    if category > LAST_TAB_CATEGORY and category != EXTRA_TAB_CATEGORY:
        return -1
    for tab, (kinds, masked) in enumerate(TAB_KINDS):
        if (item['kind'] & KIND_MASK if masked else item['kind']) & kinds:
            return tab
    return -1


def read_pictures(path, wanted):
    pictures = {}
    with open(path, 'rb') as raw:
        while True:
            header = raw.readline()
            if not header:
                break
            ident, width, height = (int(part) for part in header.split())
            pixels = raw.read(width * height * 3)
            if ident in wanted and ident not in pictures:
                out = io.BytesIO()
                Image.frombytes('RGB', (width, height), pixels).save(out, 'WEBP', quality=90, method=6)
                pictures[ident] = 'data:image/webp;base64,' + base64.b64encode(out.getvalue()).decode('ascii')
    return pictures


def main():
    objects_path, monsters_path, maps_path, pictures_path, out_path, *rest = sys.argv[1:]
    sound_path = rest[0] if rest else ''
    objects = {}
    with open(objects_path, encoding='utf-8') as lines:
        for line in lines:
            item = json.loads(line)
            item['id'] = game_id(item['id'])
            objects[item['id']] = item
    stats = {}
    with open(monsters_path, encoding='utf-8') as lines:
        for line in lines:
            monster = json.loads(line)
            stats[monster['id']] = [monster['hp'], monster['spirit'], monster['stamina'], monster['attack'],
                                    monster['defense'], monster['agility']]

    data = open(maps_path, 'rb').read()[MAPS_HEADER:]
    table = struct.unpack_from('<I', data, 4 * RECIPES_TABLE)[0]
    word = lambda at: struct.unpack_from('<H', data, table + at)[0]
    matrix = [word(2 * at) for at in range(MATRIX_SIDE * MATRIX_SIDE)]
    recipes = []
    at = RECIPES_AT
    while word(at) != 0xFFFF:
        recipes.append([word(at), word(at + 2), word(at + 4)])
        at += 6

    # The materials: what the inventory can hold that a tab lists (the 活物 among the monsters, the items).
    materials = [item for item in objects.values()
                 if tab_of(item) >= 0 and (FIRST_LATE <= item['id'] <= LAST_LATE or (0x65 <= item['id'] <= LAST_MONSTER and item['kind'] & TAB_KINDS[2][0]))]
    candidates = [item for item in objects.values()
                  if 0x65 <= item['id'] <= LAST_MONSTER or FIRST_LATE <= item['id'] <= LAST_LATE]
    shown = {item['id'] for item in materials} | {item['id'] for item in candidates} | {DEFAULT}
    shown |= {result for _, _, result in recipes}
    pictures = read_pictures(pictures_path, {objects[ident]['act'] for ident in shown if ident in objects})
    rows = []
    for ident in sorted(shown):
        if ident not in objects:
            continue
        item = objects[ident]
        rows.append([ident, item['category'], item['level'], item['kind'], item['altar'], item['name'], tab_of(item),
                     pictures.get(item['act'], ''), stats.get(ident) if ident <= LAST_MONSTER else None])

    page_data = {'objects': rows, 'matrix': matrix, 'recipes': recipes, 'categories': CATEGORY_NAMES}
    page = TEMPLATE.replace('/*DATA*/null', json.dumps(page_data, ensure_ascii=False, separators=(',', ':')))
    page = page.replace('/*SOUND*/null', json.dumps(sound_url(sound_path)))
    with open(out_path, 'w', encoding='utf-8', newline='\n') as out:
        out.write(page)
    print(f'{len(materials)} materials, {len(candidates)} candidates, {len(recipes)} recipes, {len(pictures)} pictures, '
          f'{len(page)} bytes')


def sound_url(path):
    """The cursor sound (scripts/sound/extract.py all.snd 0x2E) as a data address; none without one."""
    if not path:
        return None
    with open(path, 'rb') as sound:
        return 'data:audio/wav;base64,' + base64.b64encode(sound.read()).decode('ascii')


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>煉妖大全</title>
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
  --warn: #ff8a6a;
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
.tools { display: flex; align-items: flex-end; gap: 10px; }

.sound {
  height: 34px;
  margin: 0 0 6px;
  padding: 0 12px;
  font-family: var(--kai);
  font-size: 18px;
  color: var(--text);
  background: var(--panel);
  border: 2px solid var(--gold);
  box-shadow: 0 0 0 1px #000, inset 0 0 0 1px #000;
  cursor: pointer;
  white-space: nowrap;
}

.sound:hover { border-color: var(--gold-light); }
.sound.off { color: var(--dim); }

.search:focus { border-color: var(--gold-light); }

.layout {
  display: grid;
  grid-template-columns: 96px 250px 1fr;
  grid-template-rows: 300px 1fr;
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
    0 0 0 1px #000;
  position: relative;
}

.tabs {
  grid-column: 1;
  grid-row: 1 / span 2;
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding-top: 10px;
  margin-right: -20px;
  overflow-y: auto;
  scrollbar-width: none;
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

.tab.chosen {
  color: #181800;
  background: linear-gradient(90deg, #b8d050, #eef488);
  --edge: #b07818;
  font-weight: bold;
}

.tab.found {
  color: #20280c;
  background: linear-gradient(90deg, #7c9444, #a8bc68);
  --edge: #d0b040;
  box-shadow: inset 0 0 0 2px var(--edge), inset 6px 0 0 #e0c048;
}

.list {
  grid-column: 2;
  grid-row: 1 / span 2;
  z-index: 2;
  min-height: 0;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable both-edges;
  padding: 12px 0;
  --fill: var(--panel-list);
  scrollbar-color: var(--gold) #000;
}

.row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  font-family: var(--kai);
  font-size: 23px;
  line-height: 36px;
  padding: 0 8px 0 6px;
  cursor: pointer;
  white-space: nowrap;
  text-shadow: 1px 1px 0 #302018;
}

.row span:first-child { overflow: hidden; text-overflow: ellipsis; }
.row .level { font-family: var(--ming); font-size: 15px; color: var(--dim); flex: none; margin-left: 6px; }

/* The game's highlight: a band brightest in the middle, fading to the top and bottom edges. */
.row.chosen {
  color: #fff0b0;
  background: linear-gradient(180deg, rgba(160, 104, 0, 0), rgba(200, 130, 10, 0.95) 50%, rgba(160, 104, 0, 0));
}

.row.chosen .level { color: #fff0c0; }
.empty { font-family: var(--kai); font-size: 20px; color: var(--dim); padding: 8px 6px; }

.card {
  grid-column: 3;
  grid-row: 1;
  display: grid;
  grid-template-columns: 136px 210px 1fr;
  gap: 0 22px;
  padding: 18px 22px;
}

.card .picture { width: 136px; height: 252px; display: flex; align-items: center; justify-content: center;
  padding: 6px; background: linear-gradient(135deg, #6a6020, #3c3410); border: 2px solid #a89048;
  box-shadow: inset 0 0 0 2px #1c1806, 0 0 0 1px #000; }
.card .picture img { max-width: 100%; max-height: 100%; border: 2px solid #1c1806; background: #f4ecd8; }
.card .main { font-family: var(--kai); font-size: 20px; line-height: 28px; min-width: 0; }
.card .main .name { color: #f0e0a8; font-size: 30px; line-height: 38px; height: 38px; white-space: nowrap;
  overflow: hidden; text-overflow: ellipsis; }
.card table { border-collapse: collapse; width: 160px; font-size: 19px; }
.card td { padding: 0; height: 25px; line-height: 25px; }
.card td.value { text-align: right; font-family: var(--ming); font-size: 18px; }
.card .main .name.jump { cursor: pointer; }
.card .main .name.jump:hover { color: var(--gold-light); text-decoration: underline; }
.card .side { font-size: 17px; line-height: 30px; padding-top: 6px; }
.card .side .label { font-family: var(--kai); color: var(--dim); }
.card .side .warn { color: var(--warn); }

.rules {
  grid-column: 3;
  grid-row: 2;
  display: flex;
  flex-direction: column;
  min-height: 0;
  padding: 12px 8px 10px 18px;
}

.rules .head { flex: none; font-size: 15px; line-height: 22px; color: var(--dim); padding-bottom: 6px;
  border-bottom: 1px solid var(--gold-dark); margin-right: 10px; }
.rules .head b { font-family: var(--kai); font-size: 20px; color: var(--text); font-weight: normal; }
.rules .body { flex: 1; min-height: 0; overflow-y: auto; scrollbar-gutter: stable; padding-top: 4px;
  scrollbar-color: var(--gold) #000; }

.rule { display: grid; grid-template-columns: 1fr 26px 1fr 190px 84px; align-items: baseline;
  font-size: 18px; line-height: 34px; padding: 0 8px 0 4px; cursor: pointer; white-space: nowrap; }
.rule:hover { background: rgba(200, 130, 10, 0.2); }
.rule.open { background: rgba(200, 130, 10, 0.32); }
.rule .slot { overflow: hidden; text-overflow: ellipsis; }
.rule .slot small { font-size: 13px; color: var(--dim); margin-right: 4px; }
.rule .slot b { font-family: var(--kai); font-weight: normal; font-size: 21px; }
.rule .plus { color: var(--dim); text-align: center; }
.rule .range { text-align: right; }
.rule .count { text-align: right; color: var(--dim); font-size: 15px; }
.rule .fixed { color: var(--gold-light); }
.fallback { color: var(--dim); font-size: 13px; padding-left: 8px; }

.pairs { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 0 18px; padding: 4px 8px 10px 12px;
  font-size: 16px; line-height: 26px; border-bottom: 1px dashed rgba(168, 144, 72, 0.4); }
.pairs div { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.pairs .plus { color: var(--dim); margin: 0 4px; }
.pairs .level { font-size: 13px; color: var(--dim); margin-left: 2px; }
.pairs .more { grid-column: 1 / -1; color: var(--dim); }

.hint { flex: none; margin-top: 6px; font-size: 13px; color: #3a2008; text-align: right; }
</style>
</head>
<body>
<div class="book">
  <div class="top">
    <div class="tools">
      <input class="search" id="search" type="search" placeholder="搜尋煉化結果" autocomplete="off">
      <button class="sound" id="sound" type="button"></button>
    </div>
    <h1>煉妖大全</h1>
  </div>
  <div class="layout">
    <div class="tabs" id="tabs"></div>
    <div class="frame list" id="list"></div>
    <div class="frame card" id="card"></div>
    <div class="frame rules"><div class="head" id="head"></div><div class="body" id="rules"></div></div>
  </div>
  <div class="hint">↑↓ 選擇　Home / End 首尾　←→ 切換分類　點一條配方展開材料組合　搜尋可空格分隔多個關鍵字，Esc 清除</div>
</div>
<script>
const DATA = /*DATA*/null;

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

// Menu::AltarCombine (0x00444630).
const DEFAULT = 0x12A;
const FIRST_RESULT = 0x65;
const LAST_MONSTER = 0x1F4;
const FIRST_LATE = 0xBB9;
const LAST_LATE = 0xD47;
const KEPT_KIND = 0x800;
const NEVER_RESULT = 1;  // the object's altar: never made
const SIDE = 0x10;
const FOLDED = 0x15;  // looked up in the matrix as FOLDED_TO
const FOLDED_TO = 0xF;
const LEVEL_BONUS = 3;
const TOO_HIGH = 6;  // a result more levels than this above the first party role is refused
const STATS = ['生命', '靈力', '體力', '攻擊', '防禦', '敏捷'];
const PAIRS_SHOWN = 300;

const objects = new Map();
const byCategory = new Map();

// The objects come sorted by id: a category's candidates in the order the altar walks them.
for (const [id, category, level, kind, altar, name, tab, portrait, stats] of DATA.objects) {
  const item = { id, category, level, kind, altar, name, tab, portrait, stats };
  objects.set(id, item);

  if ((id >= FIRST_RESULT && id <= LAST_MONSTER) || (id >= FIRST_LATE && id <= LAST_LATE)) {
    if (!byCategory.has(category)) {
      byCategory.set(category, []);
    }

    byCategory.get(category).push(item);
  }
}

const materials = [...objects.values()].filter(item => item.tab >= 0);
const categoryName = category => DATA.categories[category] || `類別 ${category}`;
const fold = category => category === FOLDED ? FOLDED_TO : category;

// The object of the matrix's category for (first, second) nearest the average level plus three (0: none): the first
// at it or above, at it or below when the half sum of the categories is odd.
function search(first, second, average) {
  const category = DATA.matrix[fold(second) + SIDE * fold(first)];
  const aim = average + LEVEL_BONUS;
  const below = Math.floor((first + second) / 2) % 2 !== 0;
  let bestGap = Infinity;
  let found = 0;

  for (const item of byCategory.get(category) || []) {
    const gap = item.level - aim;
    let nearer = below ? gap <= 0 && -gap < bestGap : gap >= 0 && gap < bestGap;

    if (item.id <= LAST_MONSTER && (item.kind & KEPT_KIND) === 0) {
      nearer = false;
    }

    if (item.altar === NEVER_RESULT || !nearer) {
      continue;
    }

    found = item.id;
    bestGap = below ? -gap : gap;
  }

  return found;
}

const outcomes = new Map();

function outcome(first, second, average) {
  const key = `${first}:${second}:${average}`;

  if (!outcomes.has(key)) {
    outcomes.set(key, search(first, second, average));
  }

  return outcomes.get(key);
}

function recipeOf(first, second) {
  return DATA.recipes.find(([a, b]) => (a === first.id && b === second.id) || (a === second.id && b === first.id));
}

// Every pair once, gathered by result into rules: a fixed recipe, or the two categories and the averages that give it.
const rules = new Map();

function addPair(result, key, make, first, second) {
  if (!rules.has(result)) {
    rules.set(result, new Map());
  }

  const kept = rules.get(result);

  if (!kept.has(key)) {
    kept.set(key, make());
  }

  const rule = kept.get(key);
  rule.pairs.push([first, second]);
  rule.averages.add(Math.floor((first.level + second.level) / 2));
}

const byMaterialCategory = new Map();

for (const item of materials) {
  if (!byMaterialCategory.has(item.category)) {
    byMaterialCategory.set(item.category, []);
  }

  byMaterialCategory.get(item.category).push(item);
}

for (const first of materials) {
  for (const second of materials) {
    const recipe = recipeOf(first, second);

    if (recipe) {
      addPair(recipe[2], `r:${recipe[0]}:${recipe[1]}`, () => ({ fixed: true, pairs: [], averages: new Set() }),
        first, second);
      continue;
    }

    const average = Math.floor((first.level + second.level) / 2);
    const found = outcome(first.category, second.category, average);
    addPair(found || DEFAULT, `${first.category}:${second.category}:${found ? 1 : 0}`,
      () => ({ first: first.category, second: second.category, fallback: !found, pairs: [], averages: new Set() }),
      first, second);
  }
}

// The averages of a rule as ranges: where the altar gives the result between the lowest and highest average the
// two categories can have, kept when a pair has one of them.
function ranges(rule, result) {
  if (rule.fixed) {
    return [];
  }

  const levels = category => byMaterialCategory.get(category).map(item => item.level);
  const low = Math.floor((Math.min(...levels(rule.first)) + Math.min(...levels(rule.second))) / 2);
  const high = Math.floor((Math.max(...levels(rule.first)) + Math.max(...levels(rule.second))) / 2);
  const found = [];
  let from = null;

  for (let average = low; average <= high + 1; average++) {
    const id = average <= high ? outcome(rule.first, rule.second, average) : -1;
    const fits = rule.fallback ? id === 0 : id === result;

    if (fits && from === null) {
      from = average;
    }

    if (!fits && from !== null) {
      const to = average - 1;

      if ([...rule.averages].some(value => value >= from && value <= to)) {
        found.push([from, to]);
      }

      from = null;
    }
  }

  return found;
}

// The results by tab: their categories, in the order of the category numbers.
const tabOf = item => categoryName(item.category);
const made = [...rules.keys()].map(id => objects.get(id)).filter(item => item)
  .sort((a, b) => a.level - b.level || a.id - b.id);
const TABS = [...new Set(made.map(item => item.category))].sort((a, b) => a - b).map(categoryName);
made.forEach(item => { item.text = [item.name, tabOf(item), `${item.level}級`].join('\n'); });

let category = 0;
let chosen = 0;
let shown = [];
let opened = null;

const $ = id => document.getElementById(id);

function escape(text) {
  return text.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
}

function query() {
  return $('search').value.trim().toLowerCase().split(/\s+/).filter(word => word);
}

function drawTabs() {
  $('tabs').innerHTML = TABS.map((name, at) =>
    `<div class="tab${at === category ? ' chosen' : ''}" data-at="${at}">${name}</div>`).join('');
}

function list() {
  const words = query();
  shown = words.length ? made.filter(item => words.every(word => item.text.toLowerCase().includes(word))) :
    made.filter(item => tabOf(item) === TABS[category]);
  chosen = Math.min(chosen, Math.max(shown.length - 1, 0));
  $('list').innerHTML = shown.length ? shown.map((item, at) =>
    `<div class="row${at === chosen ? ' chosen' : ''}" data-at="${at}"><span>${escape(item.name)}</span>` +
    `<span class="level">${item.level}級</span></div>`).join('') : '<div class="empty">找不到</div>';
  opened = null;
  show();
}

function ruleList(item) {
  return [...rules.get(item.id).values()].sort((a, b) => (b.fixed ? 1 : 0) - (a.fixed ? 1 : 0) ||
    a.first - b.first || a.second - b.second || b.pairs.length - a.pairs.length);
}

function drawCard(item, kept) {
  const picture = item.portrait ? `<img src="${item.portrait}" alt="${escape(item.name)}">` : '';
  const values = item.stats ?
    `<table>${STATS.map((label, at) => `<tr><td>${label}</td><td class="value">${item.stats[at]}</td></tr>`)
      .join('')}</table>` : '';
  const need = item.level - TOO_HIGH;
  const pairs = kept.reduce((sum, rule) => sum + rule.pairs.length, 0);
  $('card').innerHTML = `<div class="picture">${picture}</div>` +
    `<div class="main"><div>${escape(tabOf(item))}</div>` +
    `<div class="name jump" title="在${item.id <= LAST_MONSTER ? '神魔異事錄' : '物品大全'}中打開">${escape(item.name)}</div>` +
    `<div>${item.level} 級</div>${values}</div>` +
    `<div class="side"><div class="label">煉化條件</div><div class="${need > 1 ? 'warn' : ''}">` +
    `${need > 1 ? `隊首角色 ${need} 級以上` : '無等級限制'}</div>` +
    `<div class="label">材料組合</div><div>${pairs} 組</div></div>`;
}

function pairText(first, second) {
  return `${escape(first.name)}<span class="level">${first.level}</span><span class="plus">＋</span>` +
    `${escape(second.name)}<span class="level">${second.level}</span>`;
}

function drawRules(item, kept) {
  $('head').innerHTML = `<b>配方</b>　第一欄、第二欄不可對調（固定配方除外）；平均＝兩樣材料等級相加除以二（捨去小數）；` +
    `祭壇須劇情開放`;
  $('rules').innerHTML = kept.map((rule, at) => {
    const range = rule.fixed ? '<span class="fixed">固定配方</span>' :
      '平均 ' + ranges(rule, item.id).map(([from, to]) => from === to ? `${from}` : `${from}～${to}`).join('、') + ' 級';
    const first = rule.fixed ? escape(rule.pairs[0][0].name) : escape(categoryName(rule.first));
    const second = rule.fixed ? escape(rule.pairs[0][1].name) : escape(categoryName(rule.second));
    const note = rule.fallback ? '<div class="fallback">（找不到對應結果時）</div>' : '';
    const head = `<div class="rule${opened === at ? ' open' : ''}" data-at="${at}">` +
      `<span class="slot"><small>第一欄</small><b>${first}</b></span><span class="plus">＋</span>` +
      `<span class="slot"><small>第二欄</small><b>${second}</b></span>` +
      `<span class="range">${range}</span>` +
      `<span class="count">${rule.pairs.length} 組</span></div>`;

    if (opened !== at) {
      return head;
    }

    const pairs = [...rule.pairs].sort((a, b) => (a[0].level + a[1].level) - (b[0].level + b[1].level));
    const more = pairs.length > PAIRS_SHOWN ? `<div class="more">……還有 ${pairs.length - PAIRS_SHOWN} 組</div>` : '';
    return head + note + `<div class="pairs">${pairs.slice(0, PAIRS_SHOWN).map(([a, b]) =>
      `<div>${pairText(a, b)}</div>`).join('')}${more}</div>`;
  }).join('');
}

// The chosen row scrolled into the list; the first and the last take the list to its very top and bottom, so that its
// margins show too.
function keepInView(row) {
  const list = $('list');

  if (chosen === 0) {
    list.scrollTop = 0;
  } else if (chosen === shown.length - 1) {
    list.scrollTop = list.scrollHeight;
  } else {
    row.scrollIntoView({ block: 'nearest' });
  }
}

function show() {
  document.querySelectorAll('.row').forEach((row, at) => row.classList.toggle('chosen', at === chosen));
  const row = document.querySelector('.row.chosen');

  if (row) {
    keepInView(row);
  }

  const item = shown[chosen];
  // While searching the tab of the result chosen is marked (found), otherwise the tab chosen.
  const searching = query().length > 0;
  document.querySelectorAll('.tab').forEach((tab, at) => {
    tab.classList.toggle('chosen', !searching && at === category);
    tab.classList.toggle('found', searching && item !== undefined && TABS[at] === tabOf(item));
  });

  if (!item) {
    ['card', 'head', 'rules'].forEach(id => { $(id).innerHTML = ''; });
    return;
  }

  keepPlace(`${item.name}@${item.id}`);

  const kept = ruleList(item);
  drawCard(item, kept);
  drawRules(item, kept);
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

  opened = null;
  show();
  $('rules').scrollTop = 0;
}

function turn(by) {
  beep();
  category = (category + by + TABS.length) % TABS.length;
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
    turn(Number(tab.dataset.at) - category);
  }
});

$('list').addEventListener('click', event => {
  const row = event.target.closest('.row');

  if (row) {
    if (Number(row.dataset.at) !== chosen) {
      beep();
    }

    chosen = Number(row.dataset.at);
    opened = null;
    show();
    $('rules').scrollTop = 0;
  }
});

// A result's name opens its page on it: a monster (the records to LAST_MONSTER) the bestiary, a later object the items
// page. The home page shows it beside its menu, alone the page itself.
function openItem(item) {
  const detail = `${item.name}@${item.id}`;
  const view = item.id <= LAST_MONSTER ? 'bestiary' : 'items';

  if (window.parent !== window) {
    window.parent.postMessage({ open: view, detail }, '*');
    return;
  }

  location.href = `${view}.html${location.search}#${encodeURIComponent(detail)}`;
}

$('card').addEventListener('click', event => {
  const name = event.target.closest('.name.jump');
  const item = shown[chosen];

  if (name && item) {
    openItem(item);
  }
});

$('rules').addEventListener('click', event => {
  const rule = event.target.closest('.rule');

  if (rule) {
    const at = Number(rule.dataset.at);
    opened = opened === at ? null : at;
    const top = rule.offsetTop;
    show();
    $('rules').scrollTop = top - $('rules').offsetTop;
  }
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

  const keys = { ArrowUp: () => move(-1), ArrowDown: () => move(1), PageUp: () => move(-15),
    PageDown: () => move(15), Home: () => move(-shown.length), End: () => move(shown.length),
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

// The result shown kept in the address (and in the home page's, which shows this page beside its menu) in place, so
// that going back to it returns here.
function keepPlace(detail) {
  history.replaceState(null, '', `#${encodeURIComponent(detail)}`);

  if (window.parent !== window) {
    window.parent.postMessage({ shown: 'altar', detail }, '*');
  }
}

function addressed() {
  try {
    return decodeURIComponent(location.hash.slice(1));
  } catch {
    return '';
  }
}

// A result named in the address (altar.html#name@id; the number tells apart those of one name) is chosen in its tab.
function chooseNamed(text = addressed()) {
  const at = text.lastIndexOf('@');
  const numbered = at > 0 && /^\d+$/.test(text.slice(at + 1));
  const name = numbered ? text.slice(0, at) : text;
  const id = numbered ? Number(text.slice(at + 1)) : null;
  const item = made.find(each => each.name === name && (id === null || each.id === id));

  if (!item) {
    return;
  }

  category = TABS.indexOf(tabOf(item));
  $('search').value = '';
  drawTabs();
  list();
  chosen = shown.indexOf(item);
  opened = null;
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
// The place read before the first list shows (and keeps) its first result.
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
