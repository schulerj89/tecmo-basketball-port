-- Intro Memory Watcher (FCEUX Lua)
--
-- Purpose:
--   Watch the original intro's sprite staging RAM and nametable writes while
--   the title images change.
--   NES CPU RAM mirrors every $0800 bytes, so emulator address $1200 is the same
--   physical RAM as $0200. This script logs the canonical $0200-$02FF OAM shadow
--   page and reports whether $1200 mirrors it in your emulator view.
--
-- Usage:
--   1. Open the private local NES image in FCEUX.
--   2. Open this script from FCEUX Lua.
--   3. Reset the game and let the intro run until the rabbit/TECMO frame changes.
--   4. Send build/emu_intro_memory_watch.ndjson back to Codex.
--
-- This writes only local ignored logs. It does not dump ROM, CHR, or ASM bytes.

local WATCH_SECONDS = 30
local MAX_WRITE_EVENTS = 1200
local MAX_PPU_EVENTS = 2400
local MAX_NAMETABLE_EVENTS = 600
local OAM_BASE = 0x0200
local OAM_MIRROR_BASE = 0x1200
local OAM_SIZE = 0x100
local VRAM_SCAN_START = 0x2000
local VRAM_SCAN_SIZE = 0x400
local VRAM_FOCUS_START = 0x2100
local VRAM_FOCUS_SIZE = 0x100
local PPU_QUEUE_BASE = 0x03A0
local PPU_QUEUE_SIZE = 0x60
local PALETTE_RAM_START = 0x3F00
local PALETTE_RAM_SIZE = 0x20

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
local STATE_FILE = PROJECT_ROOT .. "/build/emu_intro_memory_watch.ndjson"
local LOG_FILE = PROJECT_ROOT .. "/build/emu_intro_memory_watch.log"

local frame_counter = 0
local previous_oam_signature = ""
local previous_state_signature = ""
local write_event_count = 0
local ppu_event_count = 0
local nametable_event_count = 0
local write_hook_active = false
local ppu_hook_active = false
local ppu_snapshot_available = nil
local ppu_addr_latch_high = true
local ppu_addr_high = 0
local ppu_addr = nil
local ppu_ctrl = 0
local mapper_select = 0
local started_frame = nil
local previous_nametable_signature = ""
local previous_queue_signature = ""
local previous_palette_signature = ""

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
    local candidates = { "ppu", "PPU", "PpuMemory", "CIRAM", "vram" }
    for i = 1, #candidates do
      local domain = candidates[i]
      local ok, value = pcall(function() return memory.readbyte(addr, domain) end)
      if ok and value ~= nil then return value end
    end
  end
  return nil
end

local function get_register(name)
  if emu and emu.getregister then
    local ok, value = pcall(function() return emu.getregister(name) end)
    if ok and value ~= nil then return value end
  end
  return 0
end

local function current_frame()
  if emu and emu.framecount then
    local ok, value = pcall(function() return emu.framecount() end)
    if ok and value ~= nil then return value end
  end
  return frame_counter
end

local function read_state_signature()
  local watch = {
    0x0009, 0x000B, 0x000D, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E,
    0x0088, 0x008A, 0x00D7, 0x00D8, 0x00DD, 0x00DE, 0x00DF, 0x00E0,
    0x0300, 0x0301, 0x0304, 0x058D, 0x07E7
  }
  local parts = {}
  for i = 1, #watch do
    local addr = watch[i]
    parts[#parts + 1] = string.format("%s=%s", hex4(addr), hex2(readbyte_safe(addr)))
  end
  return table.concat(parts, ",")
end

local function sprite_record(slot, base)
  local addr = base + slot * 4
  return {
    y = readbyte_safe(addr),
    tile = readbyte_safe(addr + 1),
    attr = readbyte_safe(addr + 2),
    x = readbyte_safe(addr + 3)
  }
end

local function is_visible_sprite(s)
  return s.y < 0xEF
end

local function oam_signature(base, slots)
  local parts = {}
  for slot = 0, slots - 1 do
    local s = sprite_record(slot, base)
    parts[#parts + 1] = string.format("%02d:%s,%s,%s,%s",
      slot, hex2(s.y), hex2(s.tile), hex2(s.attr), hex2(s.x))
  end
  return table.concat(parts, ";")
end

local function visible_sprite_summary(base)
  local parts = {}
  for slot = 0, 63 do
    local s = sprite_record(slot, base)
    if is_visible_sprite(s) then
      parts[#parts + 1] = string.format("%02d:%s,%s,%s,%s",
        slot, hex2(s.y), hex2(s.tile), hex2(s.attr), hex2(s.x))
    end
  end
  return table.concat(parts, ";")
end

local function mirror_matches()
  for offset = 0, OAM_SIZE - 1 do
    if readbyte_safe(OAM_BASE + offset) ~= readbyte_safe(OAM_MIRROR_BASE + offset) then
      return false
    end
  end
  return true
end

local function ppu_increment()
  if (math.floor(ppu_ctrl / 4) % 2) ~= 0 then
    return 32
  end
  return 1
end

local function tile_is_intro_candidate(value)
  value = value or 0
  if value >= 0x80 and value <= 0xB8 then return true end
  if value >= 0xD7 and value <= 0xE6 then return true end
  return false
end

local function log_event(kind, detail, extra)
  local line = string.format(
    "{\"frame\":%d,\"kind\":\"%s\",\"pc\":\"%s\",\"a\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"state\":\"%s\",\"mirror_1200_matches_0200\":%s,\"detail\":\"%s\",\"visible_oam\":\"%s\"%s}",
    current_frame(),
    json_escape(kind),
    hex4(get_register("pc")),
    hex2(get_register("a")),
    hex2(get_register("x")),
    hex2(get_register("y")),
    json_escape(read_state_signature()),
    mirror_matches() and "true" or "false",
    json_escape(detail),
    json_escape(visible_sprite_summary(OAM_BASE)),
    extra or ""
  )
  append_line(STATE_FILE, line)
end

local function log_compact_event(kind, detail, extra)
  local line = string.format(
    "{\"frame\":%d,\"kind\":\"%s\",\"pc\":\"%s\",\"a\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"state\":\"%s\",\"detail\":\"%s\"%s}",
    current_frame(),
    json_escape(kind),
    hex4(get_register("pc")),
    hex2(get_register("a")),
    hex2(get_register("x")),
    hex2(get_register("y")),
    json_escape(read_state_signature()),
    json_escape(detail),
    extra or ""
  )
  append_line(STATE_FILE, line)
end

local function should_log_ppu_tile(addr, value)
  addr = addr or 0
  value = value or 0
  if addr >= 0x2100 and addr <= 0x21FF then return true end
  if addr >= PALETTE_RAM_START and addr < PALETTE_RAM_START + PALETTE_RAM_SIZE then return true end
  return tile_is_intro_candidate(value)
end

local function log_ppu_event(kind, detail, extra)
  if ppu_event_count >= MAX_PPU_EVENTS then return end
  ppu_event_count = ppu_event_count + 1
  log_compact_event(kind, detail, extra)
end

local function log_nametable_event(kind, detail, extra)
  if nametable_event_count >= MAX_NAMETABLE_EVENTS then return end
  nametable_event_count = nametable_event_count + 1
  log_compact_event(kind, detail, extra)
end

local function log_write(addr, value)
  if write_event_count >= MAX_WRITE_EVENTS then return end
  write_event_count = write_event_count + 1
  local canonical = ((addr - OAM_BASE) % OAM_SIZE) + OAM_BASE
  local slot = math.floor((canonical - OAM_BASE) / 4)
  local field_index = (canonical - OAM_BASE) % 4
  local field = ({ "y", "tile", "attr", "x" })[field_index + 1]
  local extra = string.format(
    ",\"write_addr\":\"%s\",\"canonical_addr\":\"%s\",\"slot\":%d,\"field\":\"%s\",\"value\":\"%s\",\"oam0_31\":\"%s\"",
    hex4(addr),
    hex4(canonical),
    slot,
    field,
    hex2(value),
    json_escape(oam_signature(OAM_BASE, 32))
  )
  log_event("oam_write", "write to OAM shadow or mirror", extra)
end

local function ppu_queue_signature()
  local parts = {}
  for offset = 0, PPU_QUEUE_SIZE - 1 do
    local value = readbyte_safe(PPU_QUEUE_BASE + offset)
    if tile_is_intro_candidate(value) then
      parts[#parts + 1] = string.format("%s=%s", hex4(PPU_QUEUE_BASE + offset), hex2(value))
    end
  end
  return table.concat(parts, ",")
end

local function palette_ram_signature()
  local parts = {}
  local any_read = false
  for offset = 0, PALETTE_RAM_SIZE - 1 do
    local addr = PALETTE_RAM_START + offset
    local value = read_ppu_safe(addr)
    if value ~= nil then
      any_read = true
      parts[#parts + 1] = string.format("%s=%s", hex4(addr), hex2(value))
    end
  end
  if not any_read then return "" end
  return table.concat(parts, ",")
end

local function nametable_candidate_signature()
  local parts = {}
  local any_read = false
  for offset = 0, VRAM_SCAN_SIZE - 1 do
    local addr = VRAM_SCAN_START + offset
    local value = read_ppu_safe(addr)
    if value ~= nil then
      any_read = true
      if (addr >= VRAM_FOCUS_START and addr < VRAM_FOCUS_START + VRAM_FOCUS_SIZE) or tile_is_intro_candidate(value) then
        if value ~= 0x00 and value ~= 0xFF then
          parts[#parts + 1] = string.format("%s=%s", hex4(addr), hex2(value))
        end
      end
    end
  end
  if ppu_snapshot_available == nil then
    ppu_snapshot_available = any_read
    log_nametable_event("ppu_snapshot_capability",
      any_read and "PPU nametable read API available" or "PPU nametable read API unavailable")
  end
  return table.concat(parts, ",")
end

local function register_write_hook(base)
  if not memory or not memory.registerwrite then return false end
  local ok = pcall(function()
    memory.registerwrite(base, OAM_SIZE, function(addr, size, value)
      log_write(addr or base, value or readbyte_safe(addr or base))
    end)
  end)
  return ok
end

local function handle_mapper_write(addr, value)
  if ppu_event_count >= MAX_PPU_EVENTS then return end
  if addr == 0x8000 then
    mapper_select = value or 0
    log_ppu_event("mapper_select_write", "MMC3 bank register select",
      string.format(",\"addr\":\"%s\",\"value\":\"%s\"", hex4(addr), hex2(mapper_select)))
  elseif addr == 0x8001 then
    log_ppu_event("mapper_bank_write", "MMC3 selected bank register value",
      string.format(",\"addr\":\"%s\",\"select\":\"%s\",\"value\":\"%s\"",
        hex4(addr), hex2(mapper_select), hex2(value)))
  end
end

local function handle_ppu_write(addr, value)
  local reg = 0x2000 + ((addr - 0x2000) % 8)
  value = value or readbyte_safe(addr)

  if reg == 0x2000 then
    ppu_ctrl = value
    log_ppu_event("ppu_ctrl_write", "PPUCTRL write",
      string.format(",\"addr\":\"%s\",\"value\":\"%s\",\"increment\":%d",
        hex4(addr), hex2(value), ppu_increment()))
  elseif reg == 0x2006 then
    if ppu_addr_latch_high then
      ppu_addr_high = value % 0x40
      ppu_addr_latch_high = false
      log_ppu_event("ppu_addr_high_write", "PPUADDR high byte",
        string.format(",\"addr\":\"%s\",\"value\":\"%s\"", hex4(addr), hex2(value)))
    else
      ppu_addr = ((ppu_addr_high * 0x100) + value) % 0x4000
      ppu_addr_latch_high = true
      log_ppu_event("ppu_addr_write", "PPUADDR complete",
        string.format(",\"addr\":\"%s\",\"value\":\"%s\",\"ppu_addr\":\"%s\"",
          hex4(addr), hex2(value), hex4(ppu_addr)))
    end
  elseif reg == 0x2007 then
    local current = ppu_addr
    if should_log_ppu_tile(current, value) then
      log_ppu_event("ppu_tile_write", "PPUDATA tile/palette write",
        string.format(",\"addr\":\"%s\",\"value\":\"%s\",\"ppu_addr\":\"%s\",\"increment\":%d",
          hex4(addr), hex2(value), current and hex4(current) or "????",
          ppu_increment()))
    end
    if current ~= nil then
      ppu_addr = (current + ppu_increment()) % 0x4000
    end
  end
end

local function register_ppu_hook()
  if not memory or not memory.registerwrite then return false end
  local ok_ppu = pcall(function()
    memory.registerwrite(0x2000, 8, function(addr, size, value)
      handle_ppu_write(addr or 0x2000, value or readbyte_safe(addr or 0x2000))
    end)
  end)
  local ok_mapper_select = pcall(function()
    memory.registerwrite(0x8000, 1, function(addr, size, value)
      handle_mapper_write(addr or 0x8000, value or readbyte_safe(addr or 0x8000))
    end)
  end)
  local ok_mapper_bank = pcall(function()
    memory.registerwrite(0x8001, 1, function(addr, size, value)
      handle_mapper_write(addr or 0x8001, value or readbyte_safe(addr or 0x8001))
    end)
  end)
  return ok_ppu or ok_mapper_select or ok_mapper_bank
end

local function draw_overlay()
  if not gui or not gui.text then return end
  gui.text(8, 8, "Intro memory watch")
  gui.text(8, 18, string.format("$058D=%s $88=%s $09=%s $0B=%s $0D=%s",
    hex2(readbyte_safe(0x058D)),
    hex2(readbyte_safe(0x0088)),
    hex2(readbyte_safe(0x0009)),
    hex2(readbyte_safe(0x000B)),
    hex2(readbyte_safe(0x000D))))
  gui.text(8, 28, string.format("$1200 mirror=%s writes=%d",
    mirror_matches() and "yes" or "no",
    write_event_count))
  gui.text(8, 38, string.format("ppu events=%d nt=%s",
    ppu_event_count,
    ppu_snapshot_available == nil and "?" or (ppu_snapshot_available and "yes" or "no")))
end

clear_file(STATE_FILE)
clear_file(LOG_FILE)
append_line(LOG_FILE, "intro memory watcher started")
append_line(LOG_FILE, "watching $0200-$02FF, $1200-$12FF mirror, $03A0 queue, $2006/$2007, PPU $2100 focus, and $3F00-$3F1F palette")

write_hook_active = register_write_hook(OAM_BASE)
write_hook_active = register_write_hook(OAM_MIRROR_BASE) or write_hook_active
ppu_hook_active = register_ppu_hook()
log_event("watch_started", write_hook_active and "write hooks active" or "write hooks unavailable; using per-frame diffs",
  string.format(",\"ppu_hooks_active\":%s", ppu_hook_active and "true" or "false"))

while true do
  if started_frame == nil then started_frame = current_frame() end

  local oam = oam_signature(OAM_BASE, 32)
  if oam ~= previous_oam_signature then
    previous_oam_signature = oam
    log_event("oam_frame_diff", "first 32 OAM slots changed", string.format(",\"oam0_31\":\"%s\"", json_escape(oam)))
  end

  local state = read_state_signature()
  if state ~= previous_state_signature then
    previous_state_signature = state
    log_event("state_changed", state)
  end

  local queue = ppu_queue_signature()
  if queue ~= previous_queue_signature then
    previous_queue_signature = queue
    if queue ~= "" then
      log_nametable_event("ppu_queue_candidate_changed", "candidate tile values in CPU $03A0 queue",
        string.format(",\"queue_candidates\":\"%s\"", json_escape(queue)))
    end
  end

  local nametable = nametable_candidate_signature()
  if nametable ~= previous_nametable_signature then
    previous_nametable_signature = nametable
    if nametable ~= "" then
      log_nametable_event("ppu_nametable_candidate_changed", "PPU nametable candidate/focus bytes changed",
        string.format(",\"nametable_candidates\":\"%s\"", json_escape(nametable)))
    end
  end

  local palette = palette_ram_signature()
  if palette ~= previous_palette_signature then
    previous_palette_signature = palette
    if palette ~= "" then
      log_nametable_event("ppu_palette_snapshot_changed", "PPU palette RAM bytes changed",
        string.format(",\"palette_ram\":\"%s\"", json_escape(palette)))
    end
  end

  draw_overlay()

  if WATCH_SECONDS > 0 then
    local elapsed_frames = current_frame() - started_frame
    if elapsed_frames >= WATCH_SECONDS * 60 then
      log_event("watch_complete", "time limit reached")
      append_line(LOG_FILE, "intro memory watcher complete")
      break
    end
  end

  frame_counter = frame_counter + 1
  if emu and emu.frameadvance then
    emu.frameadvance()
  else
    append_line(LOG_FILE, "ERROR emu.frameadvance unavailable")
    break
  end
end
