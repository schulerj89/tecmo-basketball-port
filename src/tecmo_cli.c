
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

static void print_usage(const char *program)
{
    printf("Usage: %s [--root PATH] COMMAND\n", program);
    printf("       %s COMMAND   # uses TECMO_DECOMP_ROOT or current directory\n", program);
    printf("\n");
    printf("Commands:\n");
    printf("  --summary               Inventory banks, lifted chunks, contracts, roster, and CHR\n");
    printf("  --banks                 Scan baseline PRG banks\n");
    printf("  --chunks                Count lifted chunks by bank and show sample chunk summaries\n");
    printf("  --assets                Analyze raw CHR bytes in build\\baseline\\Tiles.asm\n");
    printf("  --roster [TEAM|--all]   Parse labeled Bank 02 roster records\n");
    printf("  --play                  Launch native playable prototype window\n");
    printf("  --flow-test             Run headless native menu/play/quit flow checks\n");
    printf("  --controls-test         Run control-state and Win32 keyboard-mapping checks\n");
    printf("  --bank07-test           Run fixed-bank helper C counterpart checks\n");
    printf("  --video-test            Run embedded FCEUX 2.6.6 NES palette mapping checks\n");
    printf("  --music-test            Run strict TMUS parser/sequencer/synth checks\n");
    printf("  --music-source-test ROM Run the isolated strict TMUS importer gate\n");
    printf("  --frontend-audio-test   Run strict TFSX frontend cue checks\n");
    printf("  --frontend-audio-source-test ROM  Run the isolated strict TFSX importer gate\n");
    printf("  --frontend-audio-cross-pack-test FRONTEND_PACK MUSIC_PACK  Prove mixed-pack rejection\n");
    printf("  --gameplay-audio-test   Run strict TSFX/TDMC parser/mixer checks\n");
    printf("  --team-management-test  Run strict TTMG parser and native STARTERS/PLAYBOOK checks\n");
    printf("  --season-test           Run strict TSNS/TSAV season-management checks\n");
    printf("  --gameplay-state-test   Run deterministic gameplay clock/rules/shot-state checks\n");
    printf("  --gameplay-candidate-selection-test  Validate Bank06 B081/B183 receiver/defender selectors\n");
    printf("  --gameplay-scene-test PACK  Run native gameplay launch/input/shot checks\n");
    printf("  --gameplay-live-foundation-proof PACK EVENT PNG  Emit deterministic opt-in LIVE JSONL/screenshot proof (including claimant-settlement)\n");
    printf("  --gameplay-pretip-human-checkpoint PACK  Run deterministic held-B source-clocked tip handoff\n");
    printf("  --tipoff-regression-trace PACK DIR  Render continuous multi-input/matchup tip-off regression proof\n");
    printf("  --gameplay-pretip-test PACK  Validate strict TPTI-2 pre-tip assets/state\n");
    printf("  --arena-scene-test      Run native arena intro scene anchor checks\n");
    printf("  --render-test PATH      Render first playable frame to a PNG\n");
    printf("  --render-test-mode MODE PATH  Render menus, intro scenes, strict gameplay checkpoints, gameplay-live-f3-overlay, or gameplay-violation-lab-source-ITEM-frameN and gameplay-violation-lab-state-ITEM-frameN developer previews to PNG\n");
    printf("  --generate-rosters DIR  Generate static C roster source/header from Bank 02\n");
    printf("  --build-assetpack ROM PATH  Build a private .assetpack from an iNES ROM only; no decomp/capture imports\n");
    printf("  --assetpack-test       Run asset-pack builder/list/read self-tests\n");
    printf("  --gameplay-assets-test PACK  Validate strict TGPL-1 gameplay assets\n");
    printf("  --gameplay-court-test PACK  Validate strict TGCT-1 static court assets\n");
    printf("  --gameplay-court-viewport-test PACK  Validate TGCT-1 full-court decode and viewport slicing\n");
    printf("  --gameplay-court-orientation-test PACK [ROM]  Validate strict TGOR-1 state and optional Rev1 source\n");
    printf("  --gameplay-backcourt-test PACK [ROM]  Validate strict TGBC-1 detector and optional Rev1 source\n");
    printf("  --gameplay-actor-command-assignment-test PACK [ROM]  Validate bounded TGCA-1 $A023 callers/assignment evidence\n");
    printf("  --gameplay-camera-projection-test PACK  Validate strict TGCP-2 camera/projector/clamp assets\n");
    printf("  --gameplay-movement-test PACK  Validate strict TGMO-1 ordinary actor movement\n");
    printf("  --gameplay-ball-dribble-test PACK [ROM]  Validate strict TGBD-1 held-ball bounce\n");
    printf("  --gameplay-fatigue-test PACK [ROM]  Validate strict TGFT-1 fatigue state and optional Rev1 source\n");
    printf("  --gameplay-movement-harness PACK TEAM ROSTER X Y SPEED POSSESSION ORIENTATION INPUT FRAMES  Trace deterministic developer-only movement\n");
    printf("  --gameplay-cpu-steering-test PACK  Validate isolated TGAI-2 command/direction evidence\n");
    printf("  --gameplay-hud-test PACK  Validate strict THUD-1 live scoreboard assets\n");
    printf("  --gameplay-cpu-steering-inspect PACK OFFSET DX DY  Decode one command and exact direction vector (console only)\n");
    printf("  --gameplay-cpu-steering-harness PACK ACTOR POSSESSION ORIENTATION HOLDER MATCHUP DIFFICULTY X0,Y0 ... X9,Y9  Evaluate one complete court snapshot (console only)\n");
    printf("  --gameplay-cpu-steering-opcode15-harness PACK  Emit deterministic raw-only Bank06 opcode-15 lifecycle proof\n");
    printf("  --gameplay-cpu-steering-movement-harness PACK ACTOR POSSESSION ORIENTATION HOLDER MATCHUP DIFFICULTY RATING CONDITION SPEED FRAMES X0,Y0 ... X9,Y9  Feed CPU direction into TGMO (console only)\n");
    printf("  --gameplay-close-shots-test PACK  Validate strict TGCS-1 close-shot assets\n");
    printf("  --gameplay-dunk-cutaway-test PACK  Validate strict TGDK-1 dunk presentation assets\n");
    printf("  --gameplay-jump-shots-test PACK  Validate strict TGJS-2 jump-shot assets\n");
    printf("  --gameplay-shot-resolution-test PACK  Validate strict TGSR-4 shot-resolution assets\n");
    printf("  --gameplay-rebound-audit-test PACK [ROM]  Validate strict TGRB-1 fail-closed rebound eligibility diagnostics\n");
    printf("  --gameplay-penalties-test PACK  Validate strict TPNL-1 penalty rules\n");
    printf("  --gameplay-violation-referee-test PACK [ROM]  Validate strict TGVR-1 referee cutaway and optional Rev1 source\n");
    printf("  --gameplay-violation-lab-test  Validate developer-only TGVR lab input/state restoration\n");
    printf("  --gameplay-shooting-lab-test   Validate developer-only TGJS table viewer controls\n");
    printf("  --gameplay-free-throw-lineup-test PACK  Validate strict TGFL-1 raw lineup assets\n");
    printf("  --gameplay-free-throw-projection-test PACK  Validate pure TGFL-1 to TGCP-2 composition\n");
    printf("  --assetpack-list PACK  Print an asset-pack directory listing\n");
    printf("  --export-chr PATH       Export build\\baseline\\Tiles.asm to raw .chr bytes\n");
    printf("  --export-chr-png DIR    Export one PNG tile sheet per 8KB CHR bank\n");
}

int tecmo_cli_run(int argc, char **argv)
{
    const char *program = argc > 0 ? argv[0] : "tecmo_port";
    const char *env_root = getenv("TECMO_DECOMP_ROOT");
    const char *root = env_root;
    const char *command = "--summary";
    bool root_from_env = env_root != NULL && env_root[0] != '\0';
    int index = 1;
    TecmoCliContext context;

    if (!root_from_env) {
        root = ".";
    }

    if (index < argc && strcmp(argv[index], "--root") == 0) {
        if (index + 1 >= argc) {
            print_usage(program);
            return 2;
        }
        root = argv[index + 1];
        index += 2;
    }

    if (index < argc) {
        command = argv[index++];
    }
    context.root = root;
    context.command = command;
    context.argc = argc;
    context.argv = argv;
    context.index = index;

    {
        int module_result;

        module_result = tecmo_cli_run_basic_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_audio_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_management_season_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_gameplay_core_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
    }
    /* Family-specific handlers keep this entrypoint focused on routing. */
    {
        int module_result;
        module_result = tecmo_cli_run_gameplay_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_gameplay_court_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_gameplay_asset_commands(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_render_command(&context);
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
        module_result = tecmo_cli_run_asset_commands(&context);
        if (module_result == TECMO_CLI_USAGE_REQUESTED) {
            print_usage(program);
            return 2;
        }
        if (module_result != TECMO_CLI_NOT_HANDLED) return module_result;
    }






    print_usage(program);
    return 2;
}
