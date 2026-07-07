-- Intro First Sprite Watcher (FCEUX Lua)
--
-- Purpose:
--   Compare the native first-sprite trace against a real emulator run by logging
--   the OAM staging page ($0200-$02FF) and sprite counter ($058D).
--
-- Usage:
--   1. Build/run the private local NES image in FCEUX.
--   2. Open this script from FCEUX Lua.
--   3. Reset the game and let the intro begin.
--   4. Review build/emu_intro_first_sprite_watch.ndjson alongside the ignored
--      build/intro_first_sprite_trace.json report.
--
-- This writes only local ignored logs. It does not dump ROM or CHR bytes.

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
local STATE_FILE = PROJECT_ROOT .. "/build/emu_intro_first_sprite_watch.ndjson"
local LOG_FILE = PROJECT_ROOT .. "/build/emu_intro_first_sprite_watch.log"

local previous_signature = ""
local previous_sprite_count = -1
local first_nonzero_count_seen = false
local first_non_hidden_seen = false
local frame_counter = 0

local function append_line(path, line)
  local h = io.open(path, "a")
  if not h then return end
  h:write(line)
  h:write("\n")
  h:close()
end

local function clear_file(path)
  local h = io.open(path, "w")
  if not h then return end
  h:write("")
  h:close()
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
    return memory.readbyte(addr) or 0
  end
  return 0
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
    return emu.framecount()
  end
  return frame_counter
end

local function sprite_at(slot)
  local addr = 0x0200 + slot * 4
  return {
    slot = slot,
    y = readbyte_safe(addr),
    tile = readbyte_safe(addr + 1),
    attr = readbyte_safe(addr + 2),
    x = readbyte_safe(addr + 3),
  }
end

local function is_non_hidden(sprite)
  return sprite.y < 0xEF
end

local function oam_signature(limit)
  local parts = {}
  for slot = 0, limit - 1 do
    local s = sprite_at(slot)
    parts[#parts + 1] = string.format("%02d:%s,%s,%s,%s",
      slot, hex2(s.y), hex2(s.tile), hex2(s.attr), hex2(s.x))
  end
  return table.concat(parts, ";")
end

local function log_event(kind, detail)
  local line = string.format(
    "{\"frame\":%d,\"kind\":\"%s\",\"pc\":\"%s\",\"a\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"sprite_count\":\"%s\",\"detail\":\"%s\",\"oam0_15\":\"%s\"}",
    current_frame(),
    json_escape(kind),
    hex4(get_register("pc")),
    hex2(get_register("a")),
    hex2(get_register("x")),
    hex2(get_register("y")),
    hex2(readbyte_safe(0x058D)),
    json_escape(detail),
    json_escape(oam_signature(16))
  )
  append_line(STATE_FILE, line)
end

local function scan_oam_activity()
  local sprite_count = readbyte_safe(0x058D)
  if not first_nonzero_count_seen and sprite_count ~= 0 then
    first_nonzero_count_seen = true
    log_event("first_nonzero_sprite_count", "058D became nonzero")
  end

  for slot = 0, 63 do
    local s = sprite_at(slot)
    if not first_non_hidden_seen and is_non_hidden(s) then
      first_non_hidden_seen = true
      log_event("first_non_hidden_oam_seen",
        string.format("slot %02d %s,%s,%s,%s", slot, hex2(s.y), hex2(s.tile), hex2(s.attr), hex2(s.x)))
    end
  end
end

local function draw_overlay()
  if not gui or not gui.text then return end
  gui.text(8, 8, "Intro first sprite watch")
  gui.text(8, 18, string.format("058D=%s nonzero=%s visible=%s",
    hex2(readbyte_safe(0x058D)),
    first_nonzero_count_seen and "yes" or "no",
    first_non_hidden_seen and "yes" or "no"))
end

clear_file(STATE_FILE)
clear_file(LOG_FILE)
append_line(LOG_FILE, "intro first sprite watcher started")
log_event("watch_started", "waiting for $058D and OAM staging changes")

while true do
  local sprite_count = readbyte_safe(0x058D)
  local signature = oam_signature(8)

  if sprite_count ~= previous_sprite_count then
    previous_sprite_count = sprite_count
    log_event("sprite_count_changed", "058D changed")
  end

  if signature ~= previous_signature then
    previous_signature = signature
    if sprite_count ~= 0 or current_frame() < 240 then
      log_event("oam_signature_changed", signature)
    end
  end

  scan_oam_activity()
  draw_overlay()

  frame_counter = frame_counter + 1
  if emu and emu.frameadvance then
    emu.frameadvance()
  else
    append_line(LOG_FILE, "ERROR emu.frameadvance unavailable")
    break
  end
end
