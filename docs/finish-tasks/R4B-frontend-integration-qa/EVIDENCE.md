# R4B evidence and proof

## Sanitized evidence root

The principal ignored evidence record is:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/proof/r4b-frontend-integration-6b5d435/R4B-PROOF-MANIFEST.json` | 23,191 | `0AD3650262099E7B9DE2768DC47B921FB5F77E9DF254A4859A6E56E03691521B` |

Schema: `tecmo.r4b-frontend-integration-proof/TFR4BI-1`. It contains 28
selected artifact records. The independent Luna found zero missing,
mismatched, or duplicate path/size/hash records.

The manifest is ignored evidence, not a tracked payload. It sanitizes the ROM
and decompilation locations and embeds no ROM bytes, raw traces, screenshots,
or decoded proprietary content.

## Canonical build and frontend artifacts

| Evidence | Result | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| `/W4` build log | exit `0`, warnings `0` | 3,691 | `B18A903732D35D1E20C6C0BF66F3584F4925352C7CA22BE9D49E36204AAF2283` |
| Frontend suite report | `29` tests, `1` skip, `0` failures | 272,405 | `161C3005206167A5C5D83799B640D01CCA5A3195159508D27109E3C3E73E0584` |
| Combined ROM-only pack | same-tree proof input | 1,401,618 | `CC9A522A1EC5025193FD525419096D5A5AA15AF2F63A9E18E68DB8D81E87AC6F` |
| Direct asset-pack log | pass | 33 | `91241B826EAD9E662CD2B5EC100029FD758CA7F5538BBF793C246190A785352B` |
| Direct arena-scene log | pass | 37 | `A2620719FBC0C519AFE95778FC313324281FEEDBC95BE998D26A61FB94746D27` |
| Direct flow log | pass | 80 | `4143679BE4E14AE662BFB12F5A56BF2E2915D0D59BBDC302F098B4A5499512A4` |

The pack is larger than the accepted R4-only proof pack because it contains
the accepted current-main CPU/LIVE/season/audio assets as well as frontend
assets. That combined pack is the integration input used by flow and launch
checks.

## Complete frame/state proof

The replay generated exactly 3,152 fresh-process PNGs:

```text
frame-0000.png ... frame-3151.png
```

Independent and Sol checks agree:

- names contiguous with no omission or duplicate;
- every header valid PNG, width `640`, height `480`, bit depth `8`, color type
  `6` (RGBA);
- every file exactly `1,229,438` bytes;
- all `3,152` PNG hashes match the hash manifest;
- exactly `3,152` sanitized state rows, each with `global=N`;
- first and reset frames share SHA-256
  `2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A`;
- inclusive proof bound N=`4096` has accepted SHA-256
  `DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381`.

Principal source manifests:

| Artifact | SHA-256 |
| --- | --- |
| Source summary | `BB71794B7B91A83ECD8312476F424BA8EB5E1B43695B298A3A057B74AC2E1576` |
| Full state TSV | `75834BA9BC5B2249C765D9CC47DB1172CC2498EF7F40B596F86EC42C4CE75AC9` |
| Full PNG SHA manifest | `48F909F55EDF3DB9FFEEE9FC2CD288DC382F687792152894FBD43DFD0C18AE77` |
| 16-pair fresh-process determinism TSV | `C27185F825A9C9FFB989F592467C86C976CEE97C1F86B9B2EAA90BD6AFEA5B0D` |
| 9-case strict parser/bound TSV | `CA2D12FA73A0A917B672FD42D49FC01D71DCBE01AE53120018C9E6B4832363C9` |

Production boundary rows:

| N | State |
| ---: | --- |
| `949` | arena, step `8`, local `539` |
| `950` | READY entry, step `9`, local `0` |
| `1508` | finale entry, step `14`, local `0` |
| `2509` | attract entry, step `15`, local `0`, attract `1` |
| `3151` | clean reset, step `6`, local `0` |
| `4096` | inclusive safe bound, step `8`, local `535` |

This explicit N=`949`/`950` production edge resolves the independent review's
initial non-blocking observation that the canonical arena-local visual test
ends at local frame `539`: the source self-test, flow test, and complete
production frame/state proof jointly cover the frame-`540` handoff.

## Deterministic video

Both one-thread lossless encodes are byte-identical:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `full-cycle-60fps-r4b.mp4` | 1,444,648 | `9CBB34C4AD6F401103A37E06B2ABD874D86FC7F400D946DBEEF2742F2BAB8480` |
| `full-cycle-60fps-r4b-repeat.mp4` | 1,444,648 | `9CBB34C4AD6F401103A37E06B2ABD874D86FC7F400D946DBEEF2742F2BAB8480` |

The digest exactly matches accepted R4 proof. Fresh ffprobe found:

- one H.264 stream, High 4:4:4 Predictive;
- `yuv444p`, `640x480`;
- real and average rate `60/1`;
- `3,152` declared and read frames;
- duration `52.533333` seconds;
- no audio stream.

The ffprobe record SHA-256 is
`7EA49AEA0D3D2F55DC7D473AC45341F220DAF7598D4139D4D51B50455FAFE321`.
The normalized encoded-domain comparison processed 3,152 rows and returned
Y/U/V/average/min/max all `inf`. Its log SHA-256 is
`83D8B663B6456B99964D62DD96808462BD2C007B16280466701938A06975AB95`;
the stats SHA-256 is
`6F9D5AECE934F75631DAE0345F0ABE40792A3D164E00AD9F20E929FC436EECA9`.
The selected encoded-frame manifest SHA-256 is
`A62F8B0736615C5EFF61E91D4A22C74875109B1EAE2587219B2C4493E1644C3E`.

The video is intentionally silent and neutral-input. It is visual/state proof,
not input or audio proof; separate flow and audio gates cover those domains.

## Personal visual review

The Sol created and inspected these ignored contact sheets:

| Sheet | Dimensions | SHA-256 |
| --- | --- | --- |
| Opening/arena boundaries | `1280x1080` | `CC4C6F041B28C2419C413D9A5CE4719B127A13F5F7AFAD391BBC3CBB54ED5037` |
| Cards/finale boundaries | `1280x1080` | `FD6FB62D322B5A5E3D2DA2ECFF585F705D139A35F35630835A4862EEDC1E1F1C` |
| Title/attract/reset | `1280x810` | `901BE8237A6A1DDF532937676AB7B1145041A74F38E643C1CA067E7CCE000250` |

The Sol also inspected original-size source frames N=`410`, `1519`, `1606`,
`1800`, `2200`, `2800`, and `3150`, plus encoded extracts N=`410`, `1800`,
`2200`, `2800`, and `3150`.

Observed sequence: readable TECMO/license screens; continuous exact arena pan
and READY handoff; WARRIORS/CLIPPERS/BUCKS/PASS cards; clean black handoffs;
SUNS, SPURS, selector, BULLS, title write/hold/retract; attract logo cycles;
clean reset. Animated title-band clipping at sampled write/retract frames is
expected. No debug overlay, stale frame, unintended crop, discontinuity, route
jump, corrupt palette, or missing principal image was observed. The encoded
extracts visually agree with source; full-sequence PSNR provides the complete
encoded-domain check.

## Negative and contract evidence

The suite's malformed evidence is not inferred from aggregate success:

| Contract | Cases | Result |
| --- | ---: | --- |
| Finale TFIN/missing/layout/metadata/CHR/local modes | `40` | all rejected as expected |
| TFIN semantic header/screen-payload round trip | `8` | all screen payloads unchanged |
| TASG structure/flags/connector/CHR geometry | `7` | all rejected |
| TCLP CHR offset | `1` | rejected |
| BUCKS/PASS CHR offsets | `2` | rejected |
| Production strict parser/bound | `9` | all expected outcomes |

The corrupted `chr/all` fingerprint case specifically exits `1`, creates no
PNG, keeps `chr=1` while rejecting `finale=0`, and reports schema `TFIN-1`.
This distinguishes exact CHR identity failure from an absent CHR entry.

## Cross-domain integration evidence

| Gate | Result | Evidence SHA-256 |
| --- | --- | --- |
| TGAI-1 CPU steering | pass | `DAF6395D7F09BBB3097F4C969C562432DB866C6E834276DC23E365FAC525E967` |
| TGMO-1 movement | pass | `23DA97DF1A862BC64DB169F1B64A7DB567D436FB756DCCDC1ECA0D5D7C09FFA8` |
| Native flow/CLI report | pass | `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9` |
| Native flow/CLI log | pass | `0EA88139A5D2D42CA653147F8E21A7E0C4A95AD2A83E26280DF12DB62257FF65` |
| Season | pass | `536647A683939CF655DB982C23833C1B76AFC9972B7665AFC73D017559DDC1E7` |
| Music | pass | `D008B7A96080E0882A7E4DF096A35F392D7AB2209DBBC93D344A92990E5F7649` |
| Frontend audio | pass | `EF48F7605A4568347645C4C6DD3E0B3652B418B8A24C2A205DBE1FD0CE36533F` |
| Gameplay audio | pass | `AEDF1A7FE81A0A4A23E3A11420372A450D29960E140CBBE28256FE3C732020C3` |
| Win32 launch | pass | `5A6FE71A8EFF77EC7A7B937ABF114397EE4724598B582243AAA601466D1D1581` |
| Gameplay-scene/LIVE log | pass | `8402110B547030492C81D02DD788100FA3E0B1BD5D9EE4E32FEF98C1FFF312F2` |
| LIVE manifest | `DRAFT`, integration smoke only | `1F892A5FAFDCC2BAD74FF20EFB96E6196CA923EEE74BD01EEF97C664B7E6492B` |

The LIVE run covers provenance, scene controls/HUD, CPU movement, ball/fatigue,
penalties/backcourt/referee, camera/orientation, shots/dunks, halftime/final,
determinism, and missing/malformed/dependency/CHR negatives. Its two native
videos are byte-identical. `DRAFT` is preserved because the integration branch
does not satisfy the script's dedicated R1 proof-branch/original-reference
policy. It is not upgraded to formal R1 proof.

## Ownership and proprietary-artifact scan

The sanitized ownership scan SHA-256 is
`8C601DDA2182E58F3AF7CC3EA18C73B49B73FF9CA598D60EE3B5AF5A140A843B`.
It records:

- `75` current-main paths;
- `19` candidate paths;
- `0` overlap;
- first-parent merged delta equals the same `19` paths;
- `0` ownership escapes;
- `0` candidate paths with prohibited ROM/CHR/save/trace/image/audio/video/
  asset-pack/archive extensions;
- `0` binary diff rows;
- largest candidate blob is the textual frontend test runner at 167,008 bytes.

Generated ROM-derived assets and visual/video artifacts remain ignored under
`build/`. Only semantic source/tests and reports are tracked.

## Honest limits and deferred work

- The one canonical skip is the optional bounded BUCKS/PASS private-reference
  comparison; it is a skip, not a pass, and does not cover a failure in the
  semantic production path.
- This integration proof validates the native semantic-pack runtime and
  documented boundaries. It does not claim emulator-perfect parity beyond the
  accepted R4 report.
- Arena dynamic palette scheduling remains deferred; accepted static TATL
  palette behavior is unchanged.
- The original `$C4` arena wrap interpretation remains an accepted residual.
- The frontend proof video is silent and neutral-input by design.
- LIVE evidence in this task is a `DRAFT` integration gate, not new formal R1
  proof.
- No product approximation or accepted R4 limitation was upgraded by R4B.
