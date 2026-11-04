# Nexora Client CVARs

This file lists the custom cvars added to the CS 1.6 client.

## HUD

| Cvar | Default | Values | Description |
| --- | --- | --- | --- |
| `hud_style` | `0` | `0`, `1` | `0` classic HUD, `1` CS:GO-style HUD. |

### Notes
- With `hud_style 1`, the CS:GO HUD, killfeed, and ammo layout are active.
- `hud_killfade` is independent and also works with the classic HUD.

## Scoreboard

| Cvar | Default | Values | Description |
| --- | --- | --- | --- |
| `cl_scoreboard_style` | `2` | `0`, `1`, `2` | `2` uses the CS:GO-style scoreboard. Values below `2` use the classic scoreboard. |
| `cl_scoreboard_alpha` | `220` | `170-235` | Scoreboard background opacity. |
| `cl_scoreboard_show_money` | `1` | `0`, `1` | Show player money. |
| `cl_scoreboard_show_hp` | `1` | `0`, `1` | Show player health. |
| `cl_scoreboard_show_ping` | `1` | `0`, `1` | Show player ping. |
| `cl_scoreboard_show_flags` | `1` | `0`, `1` | Show country/flag information. |
| `cl_scoreboard_high_contrast` | `0` | `0`, `1` | Use high-contrast text colors. |
| `cl_scoreboard_fadein` | `0.15` | seconds | Scoreboard fade-in duration. |
| `cl_scoreboard_fadeout` | `0.18` | seconds | Scoreboard fade-out duration. |

## Killfeed

Killfeed uses the CS:GO-style layout only when `hud_style 1` is active. The fade settings below control the killfeed rows themselves; use `hud_killfade` for the screen color effect.

| Cvar | Default | Values | Description |
| --- | --- | --- | --- |
| `hud_killfeed_fade` | `1` | `0`, `1` | Enable/disable killfeed row fading. |
| `hud_killfeed_fadein` | `1` | `0`, `1` | Enable/disable row fade-in. |
| `hud_killfeed_fadeout` | `1` | `0`, `1` | Enable/disable row fade-out. |
| `hud_killfeed_fxtime` | `0.25` | seconds | Fade effect duration. |
| `hud_killfeed_holdtime` | `5` | seconds | How long each row stays on screen. |
| `hud_killfeed_color` | `""` | `"r g b"` | Text color. Empty uses team colors. |
| `hud_killfeed_alpha` | `235` | `0-255` | Text brightness/opacity. |
| `hud_killfeed_bgcolor` | `"0 0 0"` | `"r g b"` | Row background color. |
| `hud_killfeed_bgalpha` | `110` | `0-255` | Row background opacity. |

## Killfade

A `ScreenFade`-based screen color effect. It only triggers on local player kills; it does not trigger on suicide, teamkill, or non-player kills. It works independently of `hud_style`.

| Cvar | Default | Values | Description |
| --- | --- | --- | --- |
| `hud_killfade` | `1` | `0`, `1` | Enable/disable the killfade screen effect. |
| `hud_killfade_fadein` | `1` | `0`, `1` | Enable/disable the fade-in stage. |
| `hud_killfade_fadeout` | `1` | `0`, `1` | Enable/disable the fade-out stage. |
| `hud_killfade_fxtime` | `0.25` | seconds | Fade-in and fade-out duration. |
| `hud_killfade_holdtime` | `0.12` | seconds | How long the effect stays at full color. |
| `hud_killfade_color` | `"255 255 255"` | `"r g b"` | Screen fade color. |
| `hud_killfade_alpha` | `70` | `0-255` | Screen fade opacity. |

## Example

```cfg
hud_style 1

cl_scoreboard_style 2
cl_scoreboard_alpha 220
cl_scoreboard_show_money 1
cl_scoreboard_show_hp 1
cl_scoreboard_show_ping 1
cl_scoreboard_show_flags 1
cl_scoreboard_high_contrast 0
cl_scoreboard_fadein 0.15
cl_scoreboard_fadeout 0.18

hud_killfeed_fade 1
hud_killfeed_fadein 1
hud_killfeed_fadeout 1
hud_killfeed_fxtime 0.25
hud_killfeed_holdtime 5
hud_killfeed_color ""
hud_killfeed_alpha 235
hud_killfeed_bgcolor "0 0 0"
hud_killfeed_bgalpha 110

hud_killfade 1
hud_killfade_fadein 1
hud_killfade_fadeout 1
hud_killfade_fxtime 0.25
hud_killfade_holdtime 0.12
hud_killfade_color "255 255 255"
hud_killfade_alpha 70
```
