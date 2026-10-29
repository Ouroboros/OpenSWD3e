"""scripts/items/build.py <objects.jsonl> <pictures.raw> <monsters.jsonl> <out.html> [move.wav]: the swd3e items as one HTML page.

The objects are those of mon.dat 501..898 (the items and equipment) and the 活物 among 101..500 (the monsters carried as
護駕). The page states what the rebuild's verified code does with them: the item menu's tabs (Menu::ItemsList,
0x004AF904 in kCategoryKinds), the equipment slots (Party::SlotKinds) and roles (usableBy, FirstRoleBit >> role),
BattleRole::ApplyEquipment (0x0047EFE0) and Regenerate (0x0047F210), ApplyObject (0x00454D80) for an object used in
the menu (Menu::ItemsUse), the shop's 60% (kSellRate), and where the monsters drop or lose it (monsters.jsonl of
scripts/bestiary/dump).
"""

import base64
import io
import json
import sys

from PIL import Image

# The item menu's tabs (the bar of act 0x232A action 7 after 新品) and their kind bits (0x004AF904): 法寶 and 其他 take
# their bits alone, the others leave out what those two take.
TABS = [('恢復', 0x1001), ('輔助', 0x1C), ('法寶', 0x08000040), ('裝備', 0x700), ('活物', 0x800), ('其他', 0x20)]
OWN_TABS = {2, 5}
# Party::SlotKinds by the equipment menu's slot names; a slot takes an object whose kind has all its bits.
SLOTS = [
    ('武器', 0x100), ('頭部', 0x10000200), ('身體', 0x40000200), ('手部', 0x20000200), ('足部', 0x80000200),
    ('飾品', 0x400), ('法寶', 0x40), ('護駕', 0x800),
]
ARMOUR_SLOTS = {'武器', '頭部', '身體', '手部', '足部', '飾品'}  # slots 0..6: the regeneration and the element resistance
ROLES = ['陳靖仇', '于小雪', '拓跋玉兒', '張烈']  # Party::DefaultNames
FIRST_ROLE_BIT = 0x8000
ELEMENTS = ['金', '木', '水', '火', '土']
# ApplyEquipment: the bonuses added to the battle record (strength and endurance also to the attack and defence).
WORN = [('attackBonusUp', '攻擊'), ('defenseBonusUp', '防禦'), ('strengthUp', '力量'), ('enduranceUp', '耐力'),
        ('agilityUp', '敏捷'), ('wisdomUp', '智慧'), ('rapportUp', '默契'), ('evasionUp', '閃躲')]
# ApplyObject: the attributes an object used on a role raises (useKind 0 and 0x100).
USED = [('strengthUp', '力量'), ('enduranceUp', '耐力'), ('wisdomUp', '智慧'), ('agilityUp', '敏捷'),
        ('rapportUp', '默契'), ('evasionUp', '閃躲'), ('attackBonusUp', '攻擊'), ('defenseBonusUp', '防禦')]
VITALS = [('hitPointsUp', '生命'), ('spiritUp', '靈力'), ('staminaUp', '體力')]
USABLE_MASK = 6          # Menu::ItemsUse: an object with none of these bits of usableBy cannot be used
WHOLE_PARTY = 1          # SkillScript::flags: the object works on every party role
REVIVES = 0x8000         # SkillScript::ailment bit 15: a defeated role stands up
NOT_SPENT = 0x80         # ObjectData::kind: not used up
PLAIN_LIMIT = 0x1B       # ApplyObject: a category above it restores the vitals only
VITALS_USE, MAXIMA_USE, PERCENT_USE = 0, 0x100, 0x200
SELL_RATE = 60
# Database::LoadObject: the items' ids 0xBB8..0xD48 read the records 0x9C4 lower (the inventory, the shops and the
# altar use those ids, e.g. 0xC13 the bestiary book of record 591); the monsters keep their record numbers.
ITEM_ID_SHIFT = 0x9C4
FIRST_ITEM_RECORD = 501


def signed(value):
    return value - 0x10000 if value >= 0x8000 else value


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
                pictures[ident] = encode(Image.frombytes('RGB', (width, height), pixels))
    return pictures


def encode(image):
    out = io.BytesIO()
    image.save(out, 'WEBP', quality=90, method=6)
    return 'data:image/webp;base64,' + base64.b64encode(out.getvalue()).decode('ascii')


def tabs_of(kind):
    taken = []
    for at, (_, bits) in enumerate(TABS):
        if at in OWN_TABS:
            if kind & bits:
                taken.append(at)
        elif kind & bits and not kind & (TABS[2][1] | TABS[5][1]):
            taken.append(at)
    return taken


def roles_of(usable):
    roles = [name for at, name in enumerate(ROLES) if usable & (FIRST_ROLE_BIT >> at)]
    return '全員' if len(roles) == len(ROLES) else '、'.join(roles) or '無'


def joined(item, fields, unit=''):
    return '　'.join(f'{label} {signed(item[key]):+d}{unit}' for key, label in fields if item[key])


def worn(item, slots, names):
    """ApplyEquipment: the bonuses; slots 0..6 add the item's resistance of the wearer's own element (kept to 0..100)
    and, for useKind 0, regenerate each turn a percent of the maxima (at least 1, Regenerate)."""
    lines = {'加成': joined(item, WORN)}
    notes = {}
    if ARMOUR_SLOTS & set(slots):
        lines['每回合回復'] = ''
        if item['useKind'] == 0:
            lines['每回合回復'] = '　'.join(f'{label} {item[key]}%' for key, label in VITALS if item[key])
            if lines['每回合回復']:
                notes['每回合回復'] = '按上限的百分比，至少 1 點'
        lines['五行抗性'] = '　'.join(f'{ELEMENTS[at]} {value:+d}%'
                                      for at, value in enumerate(item['resistances']) if value)
        if lines['五行抗性']:
            notes['五行抗性'] = '只加穿戴者本身五行的那一項，加後保持在 0～100'
    if '武器' in slots and item['counterMax']:
        # BattleRole::Strike (0x00485F30) and StepCounterProgress (0x0048A5B0): each strike steps the weapon's counter
        # up to counterMax; a hit that does damage gains value / counterMax * unk42 (the equipment page's 熟練).
        for label in ('每回合回復', '五行抗性'):
            if not lines[label]:
                del lines[label]
        lines['熟練'] = f'命中 {item["counterMax"]} 次練滿'
        lines['熟練加成'] = f'練滿時每擊傷害 +{item["unk42"]}'
        notes['熟練加成'] = '按命中次數的比例增加，有傷害時才加'
    if '法寶' in slots and item['counterMax']:
        # The equipment page's 成長 (value / counterMax of the proficiency list); a full one with unk42 turns into
        # that object (Game::AnnounceMastery, 0x00477EE0).
        lines['成長'] = f'上限 {item["counterMax"]}'
        if item['unk42']:
            lines['成長'] += f'，練滿後變為{names.get(item["unk42"], "")}'
    return lines, notes


def used(item):
    """Menu::ItemsUse and ApplyObject: what an object used on a role in the menu does."""
    if not item['usableBy'] & USABLE_MASK:
        return {}, {}
    vitals = [(label, item[key]) for key, label in VITALS if item[key]]
    use = item['useKind']
    effect = ''
    if item['category'] > PLAIN_LIMIT or use == VITALS_USE:
        effect = '　'.join(f'{label} +{value}' for label, value in vitals)
        if use == VITALS_USE and item['category'] <= PLAIN_LIMIT:
            effect = '　'.join(part for part in (effect, joined(item, USED)) if part)
    elif use == MAXIMA_USE:
        effect = '　'.join(part for part in ('　'.join(f'{label}上限 +{value}' for label, value in vitals),
                                             joined(item, USED)) if part)
    elif use == PERCENT_USE:
        effect = '　'.join(f'{label} +{value}%' for label, value in vitals)
    if item['skillAilment'] & REVIVES:
        effect = '　'.join(part for part in ('倒下的角色復活', effect) if part)
    if not effect:
        return {}, {}
    lines = {'使用效果': effect, '對象': '我方全員' if item['skillFlags'] & WHOLE_PARTY else '我方一人'}
    percent = use == PERCENT_USE and item['category'] <= PLAIN_LIMIT
    notes = {'使用效果': '在選單中使用；按上限的百分比回復，不超過上限' if percent else
             '在選單中使用；生命、靈力、體力不超過上限，其餘為永久增加'}
    if item['kind'] & NOT_SPENT:
        lines['對象'] += '（不會用掉）'
    return lines, notes


def details(item, names):
    kind = item['kind']
    slots = [name for name, bits in SLOTS if kind & bits == bits]
    if slots:
        lines, notes = worn(item, slots, names)
    else:
        lines, notes = used(item)
    return slots, lines, notes


def sound_url(path):
    """The cursor sound (scripts/sound/extract.py all.snd 0x2E) as a data address; none without one."""
    if not path:
        return None
    with open(path, 'rb') as sound:
        return 'data:audio/wav;base64,' + base64.b64encode(sound.read()).decode('ascii')


def main():
    objects_path, pictures_path, monsters_path, out_path, *rest = sys.argv[1:]
    sound_path = rest[0] if rest else ''
    with open(objects_path, encoding='utf-8') as lines:
        objects = [json.loads(line) for line in lines]
    chosen = [item for item in objects
              if 501 <= item['id'] <= 898 or (101 <= item['id'] <= 500 and item['kind'] & SLOTS[7][1])]
    pictures = read_pictures(pictures_path, {item['act'] for item in chosen})
    names = {(item['id'] + ITEM_ID_SHIFT if item['id'] >= FIRST_ITEM_RECORD else item['id']): item['name']
             for item in objects}
    found = {}
    with open(monsters_path, encoding='utf-8') as lines:
        for line in lines:
            monster = json.loads(line)
            if not monster.get('name') or monster['hp'] <= 0:
                continue
            for key, how in (('drop', '掉落'), ('steal', '偷竊')):
                if monster.get(key):
                    found.setdefault(monster[key], {}).setdefault(how, []).append(monster['name'])

    items = []
    for item in chosen:
        tabs = tabs_of(item['kind'])
        if not tabs:
            continue
        slots, lines, notes = details(item, names)
        got = found.get(item['name'], {})
        lines['取得'] = '　'.join(f'{how}：{"、".join(dict.fromkeys(names))}' for how, names in got.items())
        items.append({
            'id': item['id'] + ITEM_ID_SHIFT if item['id'] >= FIRST_ITEM_RECORD else item['id'],
            'name': item['name'],
            'tabs': tabs,
            'slots': slots,
            'price': item['price'],
            'roles': roles_of(item['usableBy']),
            'lines': lines,
            'notes': notes,
            'description': item['description'],
            'portrait': pictures.get(item['act'], ''),
        })

    data = {'tabs': [name for name, _ in TABS], 'items': items}
    page = TEMPLATE.replace('/*DATA*/null', json.dumps(data, ensure_ascii=False, separators=(',', ':')))
    page = page.replace('/*SOUND*/null', json.dumps(sound_url(sound_path)))
    with open(out_path, 'w', encoding='utf-8', newline='\n') as out:
        out.write(page)
    print(f'{len(items)} items, {sum(1 for item in items if item["portrait"])} pictures, {len(page)} bytes')


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>物品大全</title>
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

.empty { font-family: var(--kai); font-size: 20px; color: var(--dim); padding: 8px 6px; }

.layout {
  display: grid;
  grid-template-columns: 430px 470px 230px;
  grid-template-rows: 300px 1fr 150px;
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



.shelf {
  grid-column: 1;
  grid-row: 1 / span 2;
  display: flex;
  flex-direction: column;
  min-height: 0;
}

.tabs {
  display: flex;
  gap: 2px;
  height: 40px;
  flex: none;
  padding-left: 12px;
  margin-bottom: -3px;
  z-index: 1;
}

.tab {
  font-family: var(--kai);
  font-size: 20px;
  width: 64px;
  height: 40px;
  line-height: 38px;
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
  flex: 1;
  min-height: 0;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable both-edges;
  padding: 12px 0;
  scrollbar-color: var(--gold) #000;
  --fill: var(--panel-list);
}

.row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  font-family: var(--kai);
  font-size: 25px;
  line-height: 40px;
  padding: 0 10px 0 16px;
  cursor: pointer;
  white-space: nowrap;
  text-shadow: 1px 1px 0 #302018;
}

.row .price { font-family: var(--ming); font-size: 15px; color: var(--dim); }

/* The game's highlight: a band brightest in the middle, fading to the top and bottom edges. */
.row.chosen {
  color: #fff0b0;
  background: linear-gradient(180deg, rgba(160, 104, 0, 0), rgba(200, 130, 10, 0.95) 50%, rgba(160, 104, 0, 0));
}

.row.chosen .price { color: #fff0c0; }

.stats {
  grid-column: 2;
  grid-row: 1;
  padding: 14px 24px;
  font-size: 20px;
  line-height: 34px;
  overflow: hidden;
}

.stats .kind { font-family: var(--kai); font-size: 24px; height: 34px; white-space: nowrap; overflow: hidden; }
.stats .name {
  font-family: var(--kai);
  color: #f0e0a8;
  font-size: 32px;
  height: 48px;
  line-height: 48px;
  margin-bottom: 10px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.stats table { border-collapse: collapse; table-layout: fixed; width: 100%; }
.stats td { padding: 0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.stats td:first-child { width: 110px; color: var(--dim); font-family: var(--kai); }

.portrait {
  grid-column: 3;
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

.detail {
  grid-column: 2 / span 2;
  grid-row: 2;
  display: grid;
  grid-template-columns: 130px minmax(0, 1fr);
  align-content: start;
  grid-auto-rows: 44px;
  padding: 12px 24px;
  font-size: 19px;
  overflow: hidden;
}

.detail .label { font-family: var(--kai); color: var(--dim); }
.detail > div { line-height: 44px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.detail .none { color: var(--dim); }
.detail .note { font-size: 15px; color: var(--dim); margin-left: 14px; }
.detail .found { white-space: normal; line-height: 26px; padding-top: 9px; grid-row: span 3; overflow-y: auto;
  scrollbar-color: var(--gold) #000; }

.description {
  grid-column: 1 / span 3;
  grid-row: 3;
  padding: 16px 28px;
  font-family: var(--kai);
  font-size: 28px;
  line-height: 42px;
  min-height: 0;
  overflow-x: hidden;
  overflow-y: auto;
  letter-spacing: 1px;
  white-space: pre-line;
  scrollbar-color: var(--gold) #000;
}

.hint { flex: none; margin-top: 6px; font-size: 13px; color: #3a2008; text-align: right; }

</style>
</head>
<body>
<div class="book">
  <div class="top">
    <div class="tools">
      <input class="search" id="search" type="search" placeholder="搜尋名稱、說明、效果、取得的怪物" autocomplete="off">
      <button class="sound" id="sound" type="button"></button>
    </div>
    <h1>物品大全</h1>
  </div>
  <div class="layout">
    <div class="shelf">
      <div class="tabs" id="tabs"></div>
      <div class="frame list" id="list"></div>
    </div>
    <div class="frame stats" id="stats"></div>
    <div class="portrait" id="portrait"></div>
    <div class="frame detail" id="detail"></div>
    <div class="frame description" id="description"></div>
  </div>
  <div class="hint">↑↓ 選擇　Home / End 首尾　←→ 切換分類　PageUp / PageDown 翻頁　搜尋可空格分隔多個關鍵字，Esc 清除</div>
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
let category = 0;
let chosen = 0;
let shown = [];

const $ = id => document.getElementById(id);

function escape(text) {
  return text.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
}

// Everything an item shows as text, for the search.
function searchText(item) {
  return [item.name, ...item.tabs.map(at => DATA.tabs[at]), ...item.slots, item.slots.length ? item.roles : '',
    item.description,
    ...Object.values(item.lines)].join('\n').toLowerCase();
}

DATA.items.forEach(item => { item.text = searchText(item); });

function query() {
  return $('search').value.trim().toLowerCase().split(/\s+/).filter(word => word);
}

function drawTabs() {
  $('tabs').innerHTML = DATA.tabs.map((name, at) =>
    `<div class="tab${at === category ? ' chosen' : ''}" data-at="${at}">${name}</div>`).join('');
}

function list() {
  const words = query();
  shown = words.length ? DATA.items.filter(item => words.every(word => item.text.includes(word))) :
    DATA.items.filter(item => item.tabs.includes(category));
  chosen = Math.min(chosen, Math.max(shown.length - 1, 0));
  $('list').innerHTML = shown.length ? shown.map((item, at) =>
    `<div class="row${at === chosen ? ' chosen' : ''}" data-at="${at}"><span>${escape(item.name)}</span>` +
    `<span class="price">${item.price ? `${item.price} 銀兩` : ''}</span></div>`).join('') :
    '<div class="empty">找不到</div>';
  show();
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
  // While searching the tabs of the item chosen are marked (found), otherwise the category chosen.
  const searching = query().length > 0;
  document.querySelectorAll('.tab').forEach((tab, at) => {
    tab.classList.toggle('chosen', !searching && at === category);
    tab.classList.toggle('found', searching && item !== undefined && item.tabs.includes(at));
  });

  if (!item) {
    ['stats', 'portrait', 'detail', 'description'].forEach(id => { $(id).innerHTML = ''; });
    return;
  }

  keepPlace(`${item.name}@${item.id}`);

  const kind = [...item.tabs.map(at => DATA.tabs[at]), ...item.slots].filter((name, at, all) =>
    all.indexOf(name) === at).join('・');
  // A shop sells at the price and buys at 60% of it (none without a price).
  // The number the cheat dialog adds the item by.
  const rows = [['編號', item.id], ['價格', item.price ? `${item.price} 銀兩　賣出 ${Math.floor(item.price * 60 / 100)} 銀兩` : '無法買賣']];

  // Who can wear it (the equipment menu checks the role bits; the item and battle lists read only the low bits).
  if (item.slots.length) {
    rows.push(['可裝備', item.roles]);
  }
  $('stats').innerHTML = `<div class="kind">${kind}</div><div class="name">${escape(item.name)}</div>` +
    `<table>${rows.map(([label, value]) => `<tr><td>${label}</td><td>${value}</td></tr>`).join('')}</table>`;
  $('portrait').innerHTML = item.portrait ? `<img src="${item.portrait}" alt="${escape(item.name)}">` : '';
  const line = text => text ? escape(text) : '<span class="none">無</span>';
  // The rows of the item (an equipment's four, or the damage of an object used in battle), then where it is found.
  const note = label => item.notes[label] ? `<span class="note">${escape(item.notes[label])}</span>` : '';
  $('detail').innerHTML = Object.keys(item.lines).filter(label => label !== '取得').map(label =>
    `<div class="label">${label}</div><div>${line(item.lines[label])}${note(label)}</div>`).join('') + `<div class="label">取得</div><div class="found">${line(item.lines['取得'])}</div>`;
  $('description').textContent = item.description;
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
  category = (category + by + DATA.tabs.length) % DATA.tabs.length;
  chosen = 0;
  $('search').value = '';
  drawTabs();
  list();
}

// While searching the list holds the items of every category that match all the words.
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

  if (typing && ['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) {
    return;
  }

  const keys = { ArrowUp: () => move(-1), ArrowDown: () => move(1), PageUp: () => move(-12),
    PageDown: () => move(12), Home: () => move(-shown.length), End: () => move(shown.length),
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

// The item shown kept in the address (and in the home page's, which shows this page beside its menu) in place, so
// that going back to it returns here.
function keepPlace(detail) {
  history.replaceState(null, '', `#${encodeURIComponent(detail)}`);

  if (window.parent !== window) {
    window.parent.postMessage({ shown: 'items', detail }, '*');
  }
}

function addressed() {
  try {
    return decodeURIComponent(location.hash.slice(1));
  } catch {
    return '';
  }
}

// An item named in the address (items.html#name@id, from the other pages; the number tells apart the items of one
// name, as 暗黑修羅砧) is chosen in its first tab.
function chooseNamed(text = addressed()) {
  const at = text.lastIndexOf('@');
  const numbered = at > 0 && /^\d+$/.test(text.slice(at + 1));
  const name = numbered ? text.slice(0, at) : text;
  const id = numbered ? Number(text.slice(at + 1)) : null;
  const item = DATA.items.find(each => each.name === name && (id === null || each.id === id));

  if (!item) {
    return;
  }

  category = item.tabs[0];
  $('search').value = '';
  drawTabs();
  list();
  chosen = shown.indexOf(item);
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
// The place read before the first list shows (and keeps) its first item.
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
