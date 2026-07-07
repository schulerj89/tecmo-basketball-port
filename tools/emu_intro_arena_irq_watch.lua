-- Arena IRQ/CHR Watcher (FCEUX Lua)
--
-- Purpose:
--   Narrow capture for the third intro arena screen. This script focuses on
--   MMC3 mapper slot writes, IRQ register writes, PPUCTRL/PPUSCROLL writes,
--   compact OAM facts, and small nametable row snapshots around the arena
--   bottom strip. It is intentionally smaller than emu_intro_memory_watch.lua.
--
-- Usage:
--   1. Open the private local NES image in FCEUX.
--   2. Open this script from FCEUX Lua.
--   3. Reset the game and let it run until the arena scroll starts.
--   4. Stop after the overlay says "complete" or after FCEUX stops advancing.
--
-- Output:
--   build/emu_intro_arena_irq_watch.ndjson
--   build/emu_intro_arena_irq_watch.log
--
-- This writes only local ignored logs. It does not dump ROM, CHR, or ASM bytes.

local MAX_WAIT_SECONDS = 18
local CAPTURE_FRAMES = 360
local FRAME_LOG_STRIDE = 8
local STATE_FILE_NAME = "build/emu_intro_arena_irq_watch.ndjson"
local LOG_FILE_NAME = "build/emu_intro_arena_irq_watch.log"

local mapper_select = 0
local mapper_slots = { [0] = -1, [1] = -1, [2] = -1, [3] = -1, [4] = -1, [5] = -1, [6] = -1, [7] = -1 }
local ppu_ctrl = 0
local ppu_scroll_latch_x = true
local ppu_scroll_x = 0
local ppu_scroll_y = 0
local ppu_scroll_pending_x = 0
local active = false
local active_start_frame = nil
local complete = false
local frame_counter = 0
local started_frame = nil
local last_frame_state = ""
local last_irq_write = {}
local last_ppu_ctrl = nil
local pretrigger_mapper = {}
local snapshot_offsets = { [2] = true, [30] = true, [90] = true, [180] = true, [300] = true }
local snapshots_written = {}

local function get_script_dir()
  if not debug or not debug.getinfo then return "." end
  local info = debug.getinfo(1, "S")
  if not info or not info.source then return "." end
  local src = info.source
  if string.sub(src, 1, 1) == "@" then src = string.sub(src, 2) end
  src = string.gsub(src, "\\", "/")
  return string.match(src, "^(.*)/[^/]+$") or "."
end

local function parent_dir(path)
  path = string.gsub(path or ".", "\\", "/")
  return string.match(path, "^(.*)/[^/]+$") or "."
end

local SCRIPT_DIR = get_script_dir()
local PROJECT_ROOT = parent_dir(SCRIPT_DIR)
local STATE_FILE = PROJECT_ROOT .. "/" .. STATE_FILE_NAME
local LOG_FILE = PROJECT_ROOT .. "/" .. LOG_FILE_NAME

local function append_line(path, line)
  local h = io.open(path, "a")
  if not h then return false end
  h:write(line)
  h:write("\n")
  h:close()
  return true
end

local function clear_file(path)
  local h = io.open(path, "w")
  if not h then return false end
  h:write("")
  h:close()
  return true
end

local function hex2(v) return string.format("%02X", (v or 0) % 256) end
local function hex4(v) return string.format("%04X", (v or 0) % 65536) end

local function json_escape(text)
  text = tostring(text or "")
  text = string.gsub(text, "\\", "\\\\")
  text = string.gsub(text, '"', '\\"')
  text = string.gsub(text, "\n", "\\n")
  text = string.gsub(text, "\r", "\\r")
  return text
end

local function current_frame()
  if emu and emu.framecount then
    local ok, value = pcall(function() return emu.framecount() end)
    if ok and value ~= nil then return value end
  end
  return frame_counter
end

local function current_scanline()
  if emu and emu.scanline then
    local ok, value = pcall(function() return emu.scanline() end)
    if ok and value ~= nil then return value end
  end
  if ppu and ppu.scanline then
    local ok, value = pcall(function() return ppu.scanline() end)
    if ok and value ~= nil then return value end
  end
  return -1
end

local function get_register(name)
  if emu and emu.getregister then
    local ok, value = pcall(function() return emu.getregister(name) end)
    if ok and value ~= nil then return value end
  end
  return 0
end

local function readbyte_safe(addr)
  if memory and memory.readbyte then
    local ok, value = pcall(function() return memory.readbyte(addr) end)
    if ok and value ~= nil then return value end
  end
  return 0
end

local function read_ppu_safe(addr)
  if ppu and ppu.readbyte then
    local ok, value = pcall(function() return ppu.readbyte(addr) end)
    if ok and value ~= nil then return value end
  end
  if memory and memory.readbyte then
    local domains = { "ppu", "PPU", "PpuMemory", "CIRAM", "vram" }
    for i = 1, #domains do
      local domain = domains[i]
      local ok, value = pcall(function() return memory.readbyte(addr, domain) end)
      if ok and value ~= nil then return value end
    end
  end
  return nil
end

local function state_signature()
  local watch = {
    0x0009, 0x000B, 0x000D, 0x0020, 0x0021, 0x0057, 0x0058,
    0x0088, 0x008A, 0x0300, 0x0301, 0x0304, 0x058D, 0x07E7, 0x07EB, 0x07EC
  }
  local parts = {}
  for i = 1, #watch do
    local addr = watch[i]
    parts[#parts + 1] = string.format("%s=%s", hex4(addr), hex2(readbyte_safe(addr)))
  end
  return table.concat(parts, ",")
end

local function mapper_signature()
  local parts = {}
  for i = 0, 7 do
    local value = mapper_slots[i]
    parts[#parts + 1] = string.format("R%d=%s", i, value >= 0 and hex2(value) or "??")
  end
  return table.concat(parts, ",")
end

local function visible_oam_summary()
  local count = 0
  local min_y = 255
  local max_y = 0
  local min_x = 255
  local max_x = 0
  local first = {}
  for slot = 0, 63 do
    local base = 0x0200 + slot * 4
    local y = readbyte_safe(base)
    local tile = readbyte_safe(base + 1)
    local attr = readbyte_safe(base + 2)
    local x = readbyte_safe(base + 3)
    if y < 0xEF then
      count = count + 1
      if y < min_y then min_y = y end
      if y > max_y then max_y = y end
      if x < min_x then min_x = x end
      if x > max_x then max_x = x end
      if #first < 12 then
        first[#first + 1] = string.format("%02d:%s,%s,%s,%s", slot, hex2(y), hex2(tile), hex2(attr), hex2(x))
      end
    end
  end
  if count == 0 then
    return "count=0"
  end
  return string.format("count=%d,y=%s-%s,x=%s-%s,first=%s",
    count, hex2(min_y), hex2(max_y), hex2(min_x), hex2(max_x), table.concat(first, ";"))
end

local function log_event(kind, detail, extra)
  local line = string.format(
    "{\"frame\":%d,\"scanline\":%d,\"kind\":\"%s\",\"pc\":\"%s\",\"a\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"state\":\"%s\",\"mapper\":\"%s\",\"scroll\":\"%s,%s\",\"detail\":\"%s\"%s}",
    current_frame(),
    current_scanline(),
    json_escape(kind),
    hex4(get_register("pc")),
    hex2(get_register("a")),
    hex2(get_register("x")),
    hex2(get_register("y")),
    json_escape(state_signature()),
    json_escape(mapper_signature()),
    hex2(ppu_scroll_x),
    hex2(ppu_scroll_y),
    json_escape(detail),
    extra or "")
  append_line(STATE_FILE, line)
end

local function remember_pretrigger(text)
  pretrigger_mapper[#pretrigger_mapper + 1] = text
  while #pretrigger_mapper > 24 do
    table.remove(pretrigger_mapper, 1)
  end
end

local function arena_mapper_seen()
  return mapper_slots[0] == 0x14 and mapper_slots[1] == 0x16
end

local function start_capture(reason)
  if active then return end
  active = true
  active_start_frame = current_frame()
  log_event("arena_irq_capture_started",
    reason,
    string.format(",\"pretrigger_mapper\":\"%s\"", json_escape(table.concat(pretrigger_mapper, ","))))
end

local function maybe_start_capture(reason)
  if not active and arena_mapper_seen() then
    start_capture(reason)
  end
end

local function handle_mapper_write(addr, value)
  value = value or readbyte_safe(addr)
  if addr == 0x8000 then
    mapper_select = value
    remember_pretrigger(string.format("F%d:S%d 8000=%s", current_frame(), current_scanline(), hex2(value)))
  elseif addr == 0x8001 then
    local slot = mapper_select % 8
    local previous = mapper_slots[slot]
    mapper_slots[slot] = value
    remember_pretrigger(string.format("F%d:S%d R%d=%s", current_frame(), current_scanline(), slot, hex2(value)))
    maybe_start_capture(string.format("mapper R0/R1 arena setup: %s", mapper_signature()))
    if active and previous ~= value then
      log_event("mapper_bank", "MMC3 bank-data write",
        string.format(",\"addr\":\"%s\",\"slot\":%d,\"value\":\"%s\"", hex4(addr), slot, hex2(value)))
    end
  end
end

local function handle_irq_write(addr, value)
  value = value or readbyte_safe(addr)
  if active and last_irq_write[addr] ~= value then
    last_irq_write[addr] = value
    log_event("irq_register_write", "MMC3 IRQ/control register write",
      string.format(",\"addr\":\"%s\",\"value\":\"%s\"", hex4(addr), hex2(value)))
  end
end

local function handle_ppu_write(addr, value)
  local reg = 0x2000 + ((addr - 0x2000) % 8)
  value = value or readbyte_safe(addr)

  if reg == 0x2000 then
    ppu_ctrl = value
    if active and last_ppu_ctrl ~= value then
      last_ppu_ctrl = value
      log_event("ppu_ctrl", "PPUCTRL write",
        string.format(",\"value\":\"%s\",\"nametable\":%d,\"inc\":%d",
          hex2(value), value % 4, (math.floor(value / 4) % 2) ~= 0 and 32 or 1))
    end
  elseif reg == 0x2005 then
    if ppu_scroll_latch_x then
      ppu_scroll_pending_x = value
      ppu_scroll_latch_x = false
    else
      ppu_scroll_x = ppu_scroll_pending_x
      ppu_scroll_y = value
      ppu_scroll_latch_x = true
      if active then
        log_event("ppu_scroll_xy", "PPUSCROLL pair",
          string.format(",\"x\":\"%s\",\"y\":\"%s\"", hex2(ppu_scroll_x), hex2(ppu_scroll_y)))
      end
    end
  elseif reg == 0x2006 then
    ppu_scroll_latch_x = true
  end
end

local function register_hooks()
  local ok_any = false
  if not memory or not memory.registerwrite then return false end
  local ok

  ok = pcall(function()
    memory.registerwrite(0x8000, 1, function(addr, size, value)
      handle_mapper_write(addr or 0x8000, value or readbyte_safe(addr or 0x8000))
    end)
  end)
  ok_any = ok_any or ok

  ok = pcall(function()
    memory.registerwrite(0x8001, 1, function(addr, size, value)
      handle_mapper_write(addr or 0x8001, value or readbyte_safe(addr or 0x8001))
    end)
  end)
  ok_any = ok_any or ok

  ok = pcall(function()
    memory.registerwrite(0x2000, 8, function(addr, size, value)
      handle_ppu_write(addr or 0x2000, value or readbyte_safe(addr or 0x2000))
    end)
  end)
  ok_any = ok_any or ok

  ok = pcall(function()
    memory.registerwrite(0xC000, 2, function(addr, size, value)
      handle_irq_write(addr or 0xC000, value or readbyte_safe(addr or 0xC000))
    end)
  end)
  ok_any = ok_any or ok

  ok = pcall(function()
    memory.registerwrite(0xE000, 2, function(addr, size, value)
      handle_irq_write(addr or 0xE000, value or readbyte_safe(addr or 0xE000))
    end)
  end)
  ok_any = ok_any or ok

  return ok_any
end

local function ppu_row(addr)
  local bytes = {}
  local any = false
  for i = 0, 31 do
    local value = read_ppu_safe(addr + i)
    if value ~= nil then
      any = true
      bytes[#bytes + 1] = hex2(value)
    else
      bytes[#bytes + 1] = "??"
    end
  end
  if not any then return "" end
  return hex4(addr) .. ":" .. table.concat(bytes, "")
end

local function ppu_range(addr, count)
  local bytes = {}
  local any = false
  for i = 0, count - 1 do
    local value = read_ppu_safe(addr + i)
    if value ~= nil then
      any = true
      bytes[#bytes + 1] = hex2(value)
    else
      bytes[#bytes + 1] = "??"
    end
  end
  if not any then return "" end
  return hex4(addr) .. ":" .. table.concat(bytes, "")
end

local function write_snapshot(label)
  local rows = {}
  local ranges = {}
  for row = 20, 29 do
    rows[#rows + 1] = ppu_row(0x2000 + row * 32)
  end
  for row = 0, 12 do
    rows[#rows + 1] = ppu_row(0x2400 + row * 32)
  end
  ranges[#ranges + 1] = ppu_range(0x23C0, 0x40)
  ranges[#ranges + 1] = ppu_range(0x27C0, 0x40)
  ranges[#ranges + 1] = ppu_range(0x3F00, 0x20)
  log_event("ppu_arena_rows_snapshot",
    label,
    string.format(",\"rows\":\"%s\",\"ranges\":\"%s\",\"oam\":\"%s\"",
      json_escape(table.concat(rows, ";")),
      json_escape(table.concat(ranges, ";")),
      json_escape(visible_oam_summary())))
end

local function draw_overlay()
  if not gui or not gui.text then return end
  gui.text(8, 8, "Arena IRQ/CHR watch")
  gui.text(8, 18, active and "capturing" or "waiting for R0=14 R1=16")
  gui.text(8, 28, string.format("%s scroll=%s,%s", mapper_signature(), hex2(ppu_scroll_x), hex2(ppu_scroll_y)))
  if complete then gui.text(8, 38, "complete") end
end

clear_file(STATE_FILE)
clear_file(LOG_FILE)
append_line(LOG_FILE, "arena IRQ/CHR watcher started")
append_line(LOG_FILE, "waiting for MMC3 R0=$14 and R1=$16, then capturing mapper/IRQ/scroll events and small PPU row snapshots")

local hooks_active = register_hooks()
log_event("watch_started", hooks_active and "write hooks active" or "write hooks unavailable")

while true do
  if started_frame == nil then started_frame = current_frame() end

  if active then
    local offset = current_frame() - active_start_frame
    if snapshot_offsets[offset] and not snapshots_written[offset] then
      snapshots_written[offset] = true
      write_snapshot("arena rows at +" .. tostring(offset) .. " frames")
    end

    if offset % FRAME_LOG_STRIDE == 0 then
      local frame_state = state_signature() .. "|" .. mapper_signature() .. "|" .. hex2(ppu_scroll_x) .. "," .. hex2(ppu_scroll_y)
      if frame_state ~= last_frame_state then
        last_frame_state = frame_state
        log_event("frame_state", "compact arena frame state",
          string.format(",\"oam\":\"%s\"", json_escape(visible_oam_summary())))
      end
    end

    if offset >= CAPTURE_FRAMES then
      write_snapshot("arena rows at capture end")
      log_event("watch_complete", "arena capture window reached")
      append_line(LOG_FILE, "arena IRQ/CHR watcher complete")
      complete = true
      break
    end
  elseif current_frame() - started_frame >= MAX_WAIT_SECONDS * 60 then
    log_event("watch_complete", "arena mapper setup was not seen before timeout")
    append_line(LOG_FILE, "arena IRQ/CHR watcher timed out before arena")
    complete = true
    break
  end

  draw_overlay()
  frame_counter = frame_counter + 1
  if emu and emu.frameadvance then
    emu.frameadvance()
  else
    append_line(LOG_FILE, "ERROR emu.frameadvance unavailable")
    break
  end
end
