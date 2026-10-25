# `.text` 中的 DirectInput 键盘数据格式（0x004A0036..0x004A1058）核对

- 用途：只在 0x0043BA70（DirectInput 键盘设备创建，0x0043BB82 `push offset c_dfDIKeyboard`）传给 IDirectInputDevice::SetDataFormat；该函数整体为平台替换（偏差表 0x0043BA70 行），重建不使用这段数据。
- 0x004A1040 DIDATAFORMAT：dwSize 0x18、dwObjSize 0x10、dwFlags 2（DIDF_RELAXIS）、dwDataSize 0x100、dwNumObjs 0x100、rgodf 0x004A0040。
- 0x004A0040 起 256 项 DIOBJECTDATAFORMAT：第 i 项 {pguid = 0x004ABCB0, dwOfs = i, dwType = 0x8000000C | (i << 8), dwFlags = 0}，逐项比对无差（od 读取 work/swd3e.exe 偏移 0xA0040 起 4096 字节）。
- 0x004ABCB0 GUID_Key：20 82 72 55 3C D3 CF 11 BF C7 44 45 53 54 00 00，即 {55728220-D33C-11CF-BFC7-444553540000}。
- 结论：为 dinput.lib 的标准 c_dfDIKeyboard 静态数据，内容已核对；0x004A0036..0x004A003F 为对齐填充（0xCC）。
