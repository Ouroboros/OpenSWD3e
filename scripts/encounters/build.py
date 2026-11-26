"""scripts/encounters/build.py <monsters.jsonl> <formations.jsonl> <events.jsonl> <maps.dat> <out.html> [move.wav]:
the map-encounters page (docs/encounters.html), listing the random encounters and story battles of each place with
links to the bestiary.

Random encounters come from maps.dat table 7 (encounter areas) for maps whose table 3 encounter group is non-zero
(Game::RollEncounter, 0x0040DDD0). Story battles come from events.jsonl (START_BATTLE, 0x58) traced to their maps.
"""

import base64
import json
import math
import struct
import sys
from collections import defaultdict

MAPS_HEADER = 0x200
SETTINGS_TABLE = 3
SETTINGS_BYTES = 0xE
SETTINGS_ENCOUNTER_GROUP_AT = 0xA
AREA_TABLE = 7
AREA_BYTES = 0xE
TABLE_END = 0xFFFF

CHANGE_MAP = 0x1B
SET_ROLE_TALK = 0x1C
SET_EVENT = 0x64
START_BATTLE = 0x58
OPEN_SCRIPT = 0xA1
BOSS_KIND = 0x20

# Split Babel Tower into 1st visit (before Yuwen Tuo on the roof; unreachable afterwards) and 2nd visit (demon-realm
# tower), merging their hall and roof maps so each visit is one clear entry.
BABEL_FIRST_MAPS = {282, 283, 399, 400, 401, 402, 403, 404, 405, 406, 407, 420, 582}
BABEL_SECOND_MAPS = {396, 397, 563, 564, 565, 568, 569, 570, 571, 572, 573, 574, 575, 579, 581, 583, 584}

PLACE_NOTES = {
    '通天塔（第一次）': '打倒塔頂宇文拓後無法再進入',
    '通天塔（第二次）': '魔界通天塔（古月聖滅掉妖火後進入）',
}

# Human-readable labels for multi-group places, verified against events.jsonl map transitions, mapitems.txt chests,
# and walkthrough.txt. Keyed by (place_name, tuple(sorted(set(maps)))) or (place_name, first_area_coords) for
# single-map multi-area zones.
GROUP_LABELS_BY_MAPS = {
    ('通天塔（第一次）', (399, 400)): ('二～三樓', 1),
    ('通天塔（第一次）', (401, 402, 403, 406, 407)): ('四～五樓、七樓', 2),
    ('通天塔（第一次）', (404, 405, 582)): ('八～九樓', 3),
    ('通天塔（第二次）', (568, 569)): ('二～三樓', 1),
    ('通天塔（第二次）', (397,)): ('三樓支室', 2),
    ('通天塔（第二次）', (570, 571, 572, 575)): ('四～五樓、七樓', 3),
    ('通天塔（第二次）', (396, 563, 565, 579, 581)): ('四～五樓、七樓支室', 4),
    ('通天塔（第二次）', (573, 574, 583)): ('八～九樓', 5),
    ('通天塔（第二次）', (564,)): ('九樓支室', 6),
    ('赤貫星內', (298, 299)): ('水晶迷宮第一區', 1),
    ('赤貫星內', (408, 409)): ('水晶迷宮第二區', 2),
    ('赤貫星內', (300, 301)): ('水晶迷宮第三區', 3),
    ('赤貫星內', (302, 303, 304)): ('水晶迷宮第四區', 4),
    ('赤貫星內', (410, 411)): ('水晶迷宮第五區', 5),
    ('森林軍營', (41, 309)): ('外圍', 1),
    ('森林軍營', (310,)): ('深處、近營地', 2),
    ('運輸船艙房', (55,)): ('上層艙房', 1),
    ('運輸船艙房', (505, 506)): ('底層艙房', 2),
    ('龍舟屋樑', (80, 81)): ('入口段', 1),
    ('龍舟屋樑', (82, 83, 84)): ('支路與通往大殿', 2),
    ('龍舟船艙', (345,)): ('近觀景臺', 1),
    ('龍舟船艙', (346,)): ('近偏船', 2),
    ('龍舟船艙', (347,)): ('通往屋樑', 3),
    ('魔王砦通道', (95, 96, 97)): ('前段', 1),
    ('魔王砦通道', (305, 306, 307, 308)): ('後段、近大廳', 2),
    ('採藥洞窟', (109,)): ('前段', 1),
    ('採藥洞窟', (110,)): ('深處', 2),
    ('魚肚內', (114, 117, 118)): ('前段', 1),
    ('魚肚內', (115, 116, 119)): ('後段、近氐人國', 2),
    ('貝殼海', (129, 130, 131, 132, 133, 134)): ('外段', 1),
    ('貝殼海', (527, 528, 529, 530, 531)): ('內段', 2),
    ('仙山島', (140, 142, 143)): ('桃源仙境至營地前', 1),
    ('仙山島', (145, 147)): ('營地附近', 2),
    ('仙山島', (361, 362)): ('百草澗前', 3),
    ('巴蜀古王墓', (222, 223, 224)): ('前段', 1),
    ('巴蜀古王墓', (225, 226)): ('深處', 2),
    ('皇家寶庫通道', (231,)): ('入口段', 1),
    ('皇家寶庫通道', (266,)): ('中段', 2),
    ('皇家寶庫通道', (232, 267, 268, 381, 382)): ('主廳與支室', 3),
    ('螫魂塔', (291, 292)): ('一～二樓', 1),
    ('螫魂塔', (293,)): ('三樓', 2),
    ('次元交錯空間', (296, 385, 387, 388, 389)): ('冰幻迷宮', 1),
    ('次元交錯空間', (390,)): ('中段', 2),
    ('次元交錯空間', (391,)): ('後段、近伏羲宮殿', 3),
    ('雲崗石窟山道', (321, 322, 323, 324)): ('山腳至山腰', 1),
    ('雲崗石窟山道', (325, 327, 328)): ('山腰以上', 2),
    ('月河樹林秘道', (363, 364, 365)): ('入口左半與前段', 1),
    ('月河樹林秘道', (363, 367, 368, 369)): ('入口右半與中段', 2),
    ('月河樹林秘道', (366, 370, 371)): ('深處、近月河河畔', 3),
    ('西母峰山麓', (372, 373, 374, 379)): ('外段山路', 1),
    ('西母峰山麓', (375, 376, 377, 378)): ('內段、近峰頂', 2),
    ('海中建木', (433, 434, 435)): ('下段', 1),
    ('海中建木', (436, 437, 438, 439)): ('中段', 2),
    ('海中建木', (440, 441, 442)): ('上段、近盤古頂', 3),
}

# World-map encounter zone labels, derived from CHANGE_MAP exit coordinates on maps 15, 177, 207.
WORLD_AREA_LABELS = {
    (15, (6, 33, 25, 40)): ('伏魔山、月河村附近', 1),
    (15, (12, 7, 38, 33)): ('雷夏澤、黑山鎮一帶', 2),
    (15, (39, 8, 50, 40)): ('蘆家渡、森林軍營一帶', 3),
    (15, (24, 42, 50, 59)): ('大梁城、採藥洞窟一帶', 4),
    (15, (52, 22, 89, 48)): ('豆子坑、泰山、東萊一帶', 5),
    (15, (76, 19, 79, 19)): ('東萊北方海邊（隱藏點）', 6),
    (177, (81, 26, 94, 42)): ('江都、杏山、大禹水路、長沙郡一帶', 1),
    (177, (5, 27, 30, 60)): ('長沙郡隋營、武關、寒音剎一帶', 2),
    (207, (35, 48, 59, 80)): ('長壽關、大興城一帶', 1),
    (207, (19, 50, 29, 60)): ('萌霞關附近、長壽關南方', 2),
    (207, (6, 61, 35, 114)): ('巴蜀古王墓一帶', 3),
    (207, (6, 3, 32, 48)): ('靈武郡、迷津沙一帶', 4),
    (207, (39, 33, 64, 47)): ('玄武門、河道斷崖、太原城門一帶', 5),
    (207, (33, 2, 65, 17)): ('雲崗石窟山腳一帶', 6),
    (207, (68, 45, 96, 71)): ('洛陽、通天塔一帶', 7),
}

# Story-battle sub-location labels (label, sort_order) where a place spans multiple floors/rooms.
STORY_MAP_LABELS = {
    282: ('三樓支室', 1),
    420: ('六樓大廳', 2),
    283: ('塔頂', 3),
    584: ('六樓', 1),
    293: ('三樓', 1),
    294: ('塔頂', 2),
    418: ('塔頂', 1),
    390: ('中段', 1),
    391: ('後段', 2),
    223: ('前段', 1),
}


def sound_url(path):
    """The cursor sound (scripts/sound/extract.py all.snd 0x2E) as a data address; none without one."""
    if not path:
        return None
    with open(path, 'rb') as sound:
        return 'data:audio/wav;base64,' + base64.b64encode(sound.read()).decode('ascii')


def format_map_ids(ids):
    """Compact sorted map numbers into ranges (e.g. [245,246,247] -> '245–247', [399,400] -> '399、400')."""
    ids = sorted(set(ids))
    parts = []
    start = prev = ids[0]

    for num in ids[1:]:
        if num == prev + 1:
            prev = num
            continue
        parts.append(f'{start}–{prev}' if prev - start >= 2 else '、'.join(str(x) for x in range(start, prev + 1)))
        start = prev = num

    parts.append(f'{start}–{prev}' if prev - start >= 2 else '、'.join(str(x) for x in range(start, prev + 1)))
    return '、'.join(parts)


def battle_weights(battles):
    """Preserve first-seen order and count occurrences of each battle in an encounter table."""
    counts = {}
    order = []

    for battle in battles:
        if battle not in counts:
            counts[battle] = 0
            order.append(battle)
        counts[battle] += 1

    return tuple((battle, counts[battle]) for battle in order)


def normalized_sig(rows, total):
    """Canonical signature of an encounter table's probability distribution for grouping identical maps/areas."""
    items = []

    for battle, count in rows:
        g = math.gcd(count, total)
        items.append((battle, count // g, total // g))

    return tuple(sorted(items))


def place_name_of(map_id, map_names):
    if map_id in BABEL_FIRST_MAPS:
        return '通天塔（第一次）'
    if map_id in BABEL_SECOND_MAPS:
        return '通天塔（第二次）'
    return map_names.get(map_id, f'地圖 {map_id}')


def main():
    monsters_path, formations_path, events_path, maps_path, out_path = sys.argv[1:6]
    sound_path = sys.argv[6] if len(sys.argv) > 6 else ''

    with open(monsters_path, encoding='utf-8') as f:
        records = {r['id']: r for r in (json.loads(line) for line in f if line.strip())}

    formations = {}
    with open(formations_path, encoding='utf-8') as f:
        for line in f:
            if line.strip():
                row = json.loads(line)
                if row['enemies']:
                    formations[row['battle']] = row['enemies']

    map_names = {}
    event_maps = defaultdict(set)
    events = {}
    opened_by = defaultdict(set)

    with open(events_path, encoding='utf-8') as f:
        for line in f:
            if not line.strip():
                continue
            row = json.loads(line)
            kind = row['kind']
            if kind == 'map':
                map_names[row['map']] = row['name']
            elif kind in ('role', 'region'):
                event_maps[row['event']].add(row['map'])
            elif kind == 'event':
                events[row['event']] = row

    for ev_id, ev in events.items():
        for cmd in ev['commands']:
            code, ops = cmd[0], cmd[1:]
            if code == OPEN_SCRIPT and ops:
                opened_by[ops[0]].add(ev_id)
            elif code in (SET_ROLE_TALK, SET_EVENT) and len(ops) >= 2 and ops[1] != 0:
                opened_by[ops[1]].add(ev_id)

    def maps_for_event(ev_id, visited=None):
        if visited is None:
            visited = set()
        if ev_id in visited:
            return set()
        visited.add(ev_id)
        result = set(event_maps.get(ev_id, ()))
        if not result:
            for parent in opened_by.get(ev_id, ()):
                result |= maps_for_event(parent, visited)
        return result

    with open(maps_path, 'rb') as f:
        raw = f.read()[MAPS_HEADER:]

    def word(at):
        return struct.unpack_from('<H', raw, at)[0]

    enc_group = {}
    at = struct.unpack_from('<I', raw, 4 * SETTINGS_TABLE)[0]
    while word(at) != TABLE_END:
        enc_group[word(at)] = word(at + SETTINGS_ENCOUNTER_GROUP_AT)
        at += SETTINGS_BYTES

    # Random encounter areas per map (only maps with encounterGroup > 0).
    enc_by_map = defaultdict(list)
    at = struct.unpack_from('<I', raw, 4 * AREA_TABLE)[0]
    while word(at) != TABLE_END:
        map_id = word(at)
        left, top, right, bottom = (word(at + 2 * k) for k in range(1, 5))
        table = struct.unpack_from('<I', raw, at + 0xA)[0]
        count = word(table)
        battles = [word(table + 2 * (k + 1)) for k in range(count)]
        battles = [b for b in battles if b in formations]
        if enc_group.get(map_id, 0) > 0 and battles:
            enc_by_map[map_id].append(((left, top, right, bottom), battles))
        at += AREA_BYTES

    # Story battles per map (deduplicated per map, event, battle).
    story_by_map = defaultdict(list)
    story_seen = set()
    for ev_id, ev in sorted(events.items()):
        cur_map = None
        for cmd in ev['commands']:
            code, ops = cmd[0], cmd[1:]
            if code == CHANGE_MAP and ops:
                cur_map = ops[0]
            elif code == START_BATTLE and ops:
                battle = ops[0]
                if battle not in formations:
                    continue
                mids = {cur_map} if cur_map is not None else maps_for_event(ev_id)
                for map_id in sorted(mids):
                    key = (map_id, ev_id, battle)
                    if key not in story_seen:
                        story_seen.add(key)
                        story_by_map[map_id].append((ev_id, battle))

    by_name = defaultdict(list)
    for map_id in sorted(set(enc_by_map) | set(story_by_map)):
        by_name[place_name_of(map_id, map_names)].append(map_id)

    used_monsters = set()
    used_formations = set()
    places = []

    def place_sort_key(kv):
        name, ms = kv
        # Keep 1st and 2nd Babel Tower next to each other.
        if name == '通天塔（第一次）':
            return (282, 0)
        if name == '通天塔（第二次）':
            return (282, 1)
        return (min(ms), 0)

    for place_name, map_ids in sorted(by_name.items(), key=place_sort_key):
        grouped = {}
        group_order = []
        multi_area_maps = {m for m in map_ids if len(enc_by_map.get(m, ())) > 1}

        for map_id in sorted(map_ids):
            for area, battles in enc_by_map.get(map_id, ()):
                rows = battle_weights(battles)
                total = len(battles)
                sig = normalized_sig(rows, total)
                if sig not in grouped:
                    grouped[sig] = {'maps': [], 'areas': [], 'total': total, 'rows': rows}
                    group_order.append(sig)
                grouped[sig]['maps'].append(map_id)
                if map_id in multi_area_maps:
                    grouped[sig]['areas'].append((map_id, area))

        random_groups = []
        place_monsters = set()

        for index, sig in enumerate(group_order):
            entry = grouped[sig]
            rows = entry['rows']
            for battle, _ in rows:
                used_formations.add(battle)
                for enemy in formations[battle]:
                    used_monsters.add(enemy)
                    place_monsters.add(enemy)

            maps_key = tuple(sorted(set(entry['maps'])))
            map_tag = f'地圖 {format_map_ids(entry["maps"])}'
            order = 100 + index
            title = ''
            detail = map_tag

            # Prefer explicit human-readable zone/floor names.
            world_match = None
            if entry['areas'] and len(maps_key) == 1:
                for m_id, area in entry['areas']:
                    if (m_id, area) in WORLD_AREA_LABELS:
                        world_match = WORLD_AREA_LABELS[(m_id, area)]
                        break

            if world_match is not None:
                title, order = world_match
                coords = '、'.join(f'({l},{t})–({r},{b})' for _, (l, t, r, b) in entry['areas'])
                detail = f'區域 {coords}'
            elif (place_name, maps_key) in GROUP_LABELS_BY_MAPS:
                title, order = GROUP_LABELS_BY_MAPS[(place_name, maps_key)]
                if entry['areas'] and place_name == '月河樹林秘道':
                    side = '左' if entry['areas'][0][1][0] < 50 else '右'
                    others = [m for m in maps_key if m != 363]
                    detail = f'地圖 363 {side}半、{format_map_ids(others)}'
                else:
                    detail = map_tag
            elif entry['areas']:
                area_by_map = defaultdict(list)
                for m_id, (l, t, r, b) in entry['areas']:
                    area_by_map[m_id].append(f'({l},{t})–({r},{b})')
                parts = []
                for m_id, coords in area_by_map.items():
                    prefix = f'地圖 {m_id} ' if len(map_ids) > 1 else ''
                    parts.append(f'{prefix}區域 ' + '、'.join(coords))
                other_maps = [m for m in entry['maps'] if m not in area_by_map]
                if other_maps:
                    parts.append(f'地圖 {format_map_ids(other_maps)}')
                detail = '；'.join(parts)

            random_groups.append((order, {
                'title': title,
                'detail': detail,
                'total': entry['total'],
                'rows': [[battle, count] for battle, count in rows],
            }))

        random_groups.sort(key=lambda item: item[0])
        random_list = [item[1] for item in random_groups]

        story_rows = []
        for map_id in sorted(map_ids):
            for ev_id, battle in story_by_map.get(map_id, ()):
                used_formations.add(battle)
                for enemy in formations[battle]:
                    used_monsters.add(enemy)
                    place_monsters.add(enemy)
                sub, s_order = STORY_MAP_LABELS.get(map_id, ('', 100 + map_id))
                story_rows.append((s_order, map_id, ev_id, [ev_id, map_id, battle, sub]))

        story_rows.sort(key=lambda item: (item[0], item[1], item[2]))
        story_list = [item[3] for item in story_rows]

        ordered_monsters = sorted(
            place_monsters,
            key=lambda eid: (records[eid]['level'], eid),
        )

        places.append({
            'name': place_name,
            'note': PLACE_NOTES.get(place_name, ''),
            'maps': sorted(set(map_ids)),
            'monsters': ordered_monsters,
            'random': random_list,
            'story': story_list,
        })

    monsters_data = {
        str(eid): [
            records[eid]['name'],
            records[eid]['level'],
            1 if records[eid]['kind'] & BOSS_KIND else 0,
        ]
        for eid in sorted(used_monsters)
    }
    formations_data = {
        str(battle): formations[battle]
        for battle in sorted(used_formations)
    }

    data = {
        'monsters': monsters_data,
        'formations': formations_data,
        'places': places,
    }

    page = TEMPLATE.replace('/*DATA*/null', json.dumps(data, ensure_ascii=False, separators=(',', ':')))
    page = page.replace('/*SOUND*/null', json.dumps(sound_url(sound_path)))

    with open(out_path, 'w', encoding='utf-8') as out:
        out.write(page)

    print(
        f'{len(places)} places, {len(monsters_data)} monsters, '
        f'{len(formations_data)} formations, {len(page)} bytes'
    )


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>地圖遇敵</title>
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
  --link: #f0d060;
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

.search {
  font-family: var(--ming);
  font-size: 18px;
  width: 300px;
  padding: 4px 8px;
  color: var(--text);
  background: var(--panel);
  border: 2px solid var(--gold);
  box-shadow: 0 0 0 1px #000, inset 0 0 0 1px #000;
  outline: none;
}

.search::placeholder { color: var(--dim); }
.search:focus { border-color: var(--gold-light); }

.sound {
  font-family: var(--kai);
  font-size: 18px;
  padding: 3px 10px;
  color: var(--text);
  background: var(--panel);
  border: 2px solid var(--gold);
  box-shadow: 0 0 0 1px #000, inset 0 0 0 1px #000;
  cursor: pointer;
  white-space: nowrap;
}

.sound:hover { border-color: var(--gold-light); }
.sound.off { color: var(--dim); }

.layout {
  display: grid;
  grid-template-columns: 290px 1fr;
  grid-template-rows: auto 1fr;
  gap: 10px 14px;
  margin-top: 8px;
  flex: 1;
  min-height: 0;
}

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

.list {
  grid-column: 1;
  grid-row: 1 / span 2;
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
  font-size: 22px;
  line-height: 36px;
  padding: 0 8px 0 8px;
  cursor: pointer;
  white-space: nowrap;
  text-shadow: 1px 1px 0 #302018;
}

.row span:first-child { min-width: 0; white-space: normal; overflow-wrap: anywhere; }
.row .count { font-family: var(--ming); font-size: 14px; color: var(--dim); flex: none; margin-left: 8px; }

.row.chosen {
  color: #fff0b0;
  background: linear-gradient(180deg, rgba(160, 104, 0, 0), rgba(200, 130, 10, 0.95) 50%, rgba(160, 104, 0, 0));
}

.row.chosen .count { color: #fff0c0; }
.empty { font-family: var(--kai); font-size: 20px; color: var(--dim); padding: 8px 10px; }

.overview {
  grid-column: 2;
  grid-row: 1;
  padding: 14px 22px 16px;
  max-height: 250px;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable;
  scrollbar-color: var(--gold) #000;
}

.place-head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 8px;
  margin-bottom: 10px;
  border-bottom: 1px solid rgba(168, 144, 72, 0.4);
}

.place-title {
  font-family: var(--kai);
  font-size: 30px;
  line-height: 36px;
  color: #f0e0a8;
}

.place-meta {
  font-size: 15px;
  color: var(--dim);
  white-space: nowrap;
}

.place-note {
  font-size: 15px;
  color: #e8c070;
  margin-bottom: 10px;
}

.chips {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 10px;
}

a.chip {
  display: inline-flex;
  align-items: baseline;
  gap: 6px;
  padding: 3px 10px;
  color: var(--link);
  text-decoration: none;
  background: rgba(168, 144, 72, 0.1);
  border: 1px solid rgba(168, 144, 72, 0.45);
  font-size: 17px;
  line-height: 24px;
}

a.chip:hover {
  color: var(--gold-light);
  border-color: var(--gold-light);
  background: rgba(240, 232, 72, 0.12);
}

a.chip .lv { font-size: 14px; color: var(--dim); }

.boss {
  display: inline-block;
  padding: 0 4px;
  font-family: var(--ming);
  font-size: 12px;
  line-height: 17px;
  color: #ffd0b0;
  background: #5a1c10;
  border: 1px solid #a84830;
  vertical-align: middle;
}

.detail {
  grid-column: 2;
  grid-row: 2;
  min-height: 0;
  padding: 14px 22px 16px;
  overflow-x: hidden;
  overflow-y: auto;
  scrollbar-gutter: stable;
  scrollbar-color: var(--gold) #000;
}

.group { margin-bottom: 18px; }
.group:last-child { margin-bottom: 0; }

.group-head {
  font-family: var(--kai);
  font-size: 21px;
  line-height: 30px;
  color: #f0e0a8;
  padding-left: 10px;
  border-left: 3px solid var(--gold);
  margin-bottom: 8px;
}

.group-head .sub {
  font-family: var(--ming);
  font-size: 15px;
  color: var(--dim);
  margin-left: 10px;
}

table {
  width: 100%;
  border-collapse: collapse;
  font-size: 17px;
  line-height: 28px;
}

th, td {
  text-align: left;
  vertical-align: top;
  padding: 5px 10px;
  border-bottom: 1px solid rgba(168, 144, 72, 0.3);
}

th {
  font-family: var(--kai);
  font-size: 18px;
  font-weight: normal;
  color: var(--dim);
  border-bottom-color: rgba(168, 144, 72, 0.55);
}

td.battle { width: 88px; white-space: nowrap; color: var(--dim); }
td.rate { width: 112px; white-space: nowrap; color: #f0e0a8; }
td.event { width: 210px; white-space: nowrap; color: var(--dim); }
td.enemies { overflow-wrap: anywhere; }

a.mlink {
  color: var(--link);
  text-decoration: none;
}

a.mlink:hover {
  color: var(--gold-light);
  text-decoration: underline;
}

.lv-inline { font-size: 14px; color: var(--dim); margin-left: 2px; }
.times { color: #f0e0a8; margin-left: 3px; }
.plus { color: var(--dim); margin: 0 7px; }

.hit {
  background: rgba(240, 210, 72, 0.24) !important;
  border-color: var(--gold-light) !important;
  color: var(--gold-light) !important;
}

a.mlink.hit {
  padding: 1px 4px;
  border: 1px solid var(--gold-light);
}

.hint { flex: none; margin-top: 6px; font-size: 13px; color: #3a2008; text-align: right; }
</style>
</head>
<body>
<div class="book">
  <div class="top">
    <div class="tools">
      <input class="search" id="search" type="search" placeholder="搜尋地圖、地標、怪物名稱" autocomplete="off">
      <button class="sound" id="sound" type="button"></button>
    </div>
    <h1>地圖遇敵</h1>
  </div>
  <div class="layout">
    <div class="frame list" id="list"></div>
    <div class="frame overview" id="overview"></div>
    <div class="frame detail" id="detail"></div>
  </div>
  <div class="hint">↑↓ 選擇　Home / End 首尾　PgUp / PgDn 翻頁　點怪物名稱開啟神魔異事錄　搜尋可空格分隔關鍵字，Esc 清除</div>
</div>
<script>
const DATA = /*DATA*/null;
const MOVE_SOUND = /*SOUND*/null;

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

const $ = id => document.getElementById(id);

function escape(text) {
  return String(text).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
}

function gcd(a, b) {
  while (b) {
    [a, b] = [b, a % b];
  }
  return a;
}

function formatRate(count, total) {
  if (count >= total) {
    return '100%';
  }

  const g = gcd(count, total);
  const pct = Math.round((count * 100) / total);
  return `${count / g}/${total / g} (${pct}%)`;
}

function monsterOf(id) {
  const [name, level, boss] = DATA.monsters[String(id)];
  return { id, name, level, boss };
}

function monsterHref(m) {
  return `bestiary.html${location.search}#${encodeURIComponent(`${m.name}@${m.id}`)}`;
}

// Search matches place name, notes, sub-area titles, map numbers, and every monster name encountered there.
DATA.places.forEach(place => {
  const names = place.monsters.map(id => DATA.monsters[String(id)][0]);
  const mapTags = place.maps.map(m => `地圖${m} 地圖 ${m}`);
  const groupTags = place.random.map(g => `${g.title} ${g.detail}`);
  const storyTags = place.story.map(s => s[3]);
  place.text = [place.name, place.note, ...names, ...mapTags, ...groupTags, ...storyTags].join('\n').toLowerCase();
});

let shown = [];
let chosen = 0;

function query() {
  return $('search').value.trim().toLowerCase().split(/\s+/).filter(Boolean);
}

function monsterHit(m, words) {
  if (!words.length) {
    return false;
  }

  const lower = m.name.toLowerCase();
  return words.some(word => lower.includes(word));
}

function keepInView(row) {
  const box = $('list');

  if (chosen === 0) {
    box.scrollTop = 0;
  } else if (chosen === shown.length - 1) {
    box.scrollTop = box.scrollHeight;
  } else {
    row.scrollIntoView({ block: 'nearest' });
  }
}

function renderEnemies(battle, words) {
  const ids = DATA.formations[String(battle)] || [];
  const counts = new Map();
  const order = [];

  for (const id of ids) {
    if (!counts.has(id)) {
      counts.set(id, 0);
      order.push(id);
    }

    counts.set(id, counts.get(id) + 1);
  }

  return order.map(id => {
    const m = monsterOf(id);
    const n = counts.get(id);
    const hit = monsterHit(m, words) ? ' hit' : '';
    const boss = m.boss ? ' <span class="boss">BOSS</span>' : '';
    const times = n > 1 ? `<span class="times">×${n}</span>` : '';
    return `<a class="mlink${hit}" href="${monsterHref(m)}" data-id="${m.id}">${escape(m.name)}</a>` +
      `<span class="lv-inline">(${m.level}級)</span>${boss}${times}`;
  }).join('<span class="plus">＋</span>');
}

function show() {
  document.querySelectorAll('.row').forEach((row, at) => row.classList.toggle('chosen', at === chosen));
  const row = document.querySelector('.row.chosen');

  if (row) {
    keepInView(row);
  }

  const place = shown[chosen];

  if (!place) {
    $('overview').innerHTML = '';
    $('detail').innerHTML = '';
    return;
  }

  keepPlace(place.name);
  const words = query();
  const kinds = [];

  if (place.random.length) {
    kinds.push('隨機遇敵');
  }

  if (place.story.length) {
    kinds.push('劇情戰鬥');
  }

  const chips = place.monsters.map(id => {
    const m = monsterOf(id);
    const hit = monsterHit(m, words) ? ' hit' : '';
    const boss = m.boss ? '<span class="boss">BOSS</span>' : '';
    return `<a class="chip${hit}" href="${monsterHref(m)}" data-id="${m.id}">` +
      `<span>${escape(m.name)}</span><span class="lv">${m.level}級</span>${boss}</a>`;
  }).join('');

  const note = place.note ? `<div class="place-note">${escape(place.note)}</div>` : '';

  $('overview').innerHTML =
    `<div class="place-head"><div class="place-title">${escape(place.name)}</div>` +
    `<div class="place-meta">共 ${place.monsters.length} 種怪物 · ${kinds.join('、')}</div></div>` +
    `${note}<div class="chips">${chips}</div>`;

  const blocks = [];

  for (const group of place.random) {
    const head = group.title
      ? `${escape(group.title)}<span class="sub">${escape(group.detail)}</span>`
      : `隨機遇敵<span class="sub">${escape(group.detail)}</span>`;
    const trs = group.rows.map(([battle, count]) =>
      `<tr><td class="battle">第 ${battle} 場</td>` +
      `<td class="rate">${formatRate(count, group.total)}</td>` +
      `<td class="enemies">${renderEnemies(battle, words)}</td></tr>`).join('');
    blocks.push(
      `<div class="group"><div class="group-head">${head}</div>` +
      `<table><tr><th>戰鬥</th><th>機率</th><th>敵人</th></tr>${trs}</table></div>`
    );
  }

  if (place.story.length) {
    const multiMap = place.maps.length > 1;
    const trs = place.story.map(([ev, mapId, battle, sub]) => {
      const where = sub ? `${sub} · 事件 ${ev}` : `事件 ${ev}${multiMap ? `（地圖 ${mapId}）` : ''}`;
      return `<tr><td class="event">${escape(where)}</td>` +
        `<td class="battle">第 ${battle} 場</td>` +
        `<td class="enemies">${renderEnemies(battle, words)}</td></tr>`;
    }).join('');
    blocks.push(
      `<div class="group"><div class="group-head">劇情戰鬥</div>` +
      `<table><tr><th>觸發</th><th>戰鬥</th><th>敵人</th></tr>${trs}</table></div>`
    );
  }

  $('detail').innerHTML = blocks.join('');
  const firstHit = $('detail').querySelector('.hit');

  if (firstHit) {
    const group = firstHit.closest('.group');

    if (group) {
      group.scrollIntoView({ block: 'nearest' });
    }
  }
}

function list() {
  const words = query();
  shown = words.length
    ? DATA.places.filter(place => words.every(word => place.text.includes(word)))
    : DATA.places;
  chosen = Math.min(chosen, Math.max(shown.length - 1, 0));
  $('list').innerHTML = shown.length
    ? shown.map((place, at) =>
        `<div class="row${at === chosen ? ' chosen' : ''}" data-at="${at}">` +
        `<span>${escape(place.name)}</span><span class="count">${place.monsters.length} 種</span></div>`).join('')
    : '<div class="empty">找不到</div>';
  show();
}

function move(by) {
  if (!shown.length) {
    return;
  }

  const before = chosen;
  chosen = Math.max(0, Math.min(shown.length - 1, chosen + by));

  if (chosen !== before) {
    beep();
    $('detail').scrollTop = 0;
  }

  show();
}

function openMonster(id) {
  const m = monsterOf(id);
  const detail = `${m.name}@${m.id}`;

  if (window.parent !== window) {
    window.parent.postMessage({ open: 'bestiary', detail }, '*');
    return;
  }

  location.href = `bestiary.html${location.search}#${encodeURIComponent(detail)}`;
}

document.addEventListener('click', event => {
  const link = event.target.closest('a.chip, a.mlink');

  if (!link) {
    return;
  }

  event.preventDefault();
  openMonster(Number(link.dataset.id));
});

$('list').addEventListener('click', event => {
  const row = event.target.closest('.row');

  if (row) {
    const at = Number(row.dataset.at);

    if (at !== chosen) {
      beep();
      $('detail').scrollTop = 0;
    }

    chosen = at;
    show();
  }
});

$('search').addEventListener('input', () => {
  chosen = 0;
  $('detail').scrollTop = 0;
  list();
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
    PageUp: () => move(-15),
    PageDown: () => move(15),
    Home: () => move(-shown.length),
    End: () => move(shown.length),
  };

  if (keys[event.key]) {
    keys[event.key]();
    event.preventDefault();
  }
});

function fit() {
  const book = document.querySelector('.book');
  const scale = Math.min(innerWidth / book.offsetWidth, innerHeight / book.offsetHeight);
  const left = (innerWidth - book.offsetWidth * scale) / 2;
  const top = (innerHeight - book.offsetHeight * scale) / 2;
  book.style.transform = `translate(${left}px, ${top}px) scale(${scale})`;
}

function keepPlace(detail) {
  history.replaceState(null, '', `${location.search}#${encodeURIComponent(detail)}`);

  if (window.parent !== window) {
    window.parent.postMessage({ shown: 'encounters', detail }, '*');
  }
}

function addressed() {
  try {
    return decodeURIComponent(location.hash.slice(1));
  } catch {
    return '';
  }
}

function chooseNamed(text = addressed()) {
  if (!text) {
    return;
  }

  const at = DATA.places.findIndex(place => place.name === text);

  if (at < 0) {
    return;
  }

  $('search').value = '';
  list();
  chosen = at;
  $('detail').scrollTop = 0;
  show();
}

function drawSound() {
  const button = $('sound');
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

$('sound').addEventListener('click', event => {
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
list();
chooseNamed(place);
</script>
</body>
</html>
'''

if __name__ == '__main__':
    main()
