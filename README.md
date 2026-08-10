# AREA512

<p align="center">
  <img src="image/area512.png" alt="AREA512" width="480" />
</p>

Welcome to AREA512!

AREA512 is an OS built for the Cardputer ADV and Cardputer v1.1,
a tiny device with just 512KB of RAM and 8MB of flash storage!

It is based on FemtoRuby,
so you can write Ruby right on the Cardputer,
then compile and run it — all on the device!

## Quick Install

All you need is esptool:

```sh
pip install esptool

# Cardputer ADV
esptool.py -c esp32s3 -b 460800 write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 firmware/Area512Adv.bin

# Cardputer v1.1
esptool.py -c esp32s3 -b 460800 write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 firmware/Area512V11.bin
```

- If the port is not auto-detected, add `-p /dev/ttyACM0` to the `esptool.py` command.
- Insert a FAT32-formatted microSD card into the Cardputer (it is used to store app data).

## Using AREA512

![The AREA512 file manager running on a Cardputer ADV](image/cardputer.jpg)

The screen shows a listing of the current directory: directories first, then applications (`.rb` / `.mrb` are merged into a single entry without the extension), then other files.

The following keys are available.

| Key | Action |
| --- | --- |
| `;` / `.` (or `k` / `j`) | Move the cursor up / down |
| `/` or Enter | Open (enter a directory / run an app / view a Markdown file) |
| `,` or BS | Go to the parent directory |
| `1`–`9` | Jump to the n-th entry |
| `e` | Edit the selected file |
| `c` | Compile the selected app's `.rb` |
| `a` | Compile every `.rb` in the current directory |
| `R` | Run the selected directory as an application |
| `N` | Create a new file (you type the name) |
| `K` | Create a new directory (you type the name) |
| `x` | Delete (asks `y/n` for confirmation) |
| `m` | Move the selected entry (you type the destination path) |
| `r` | Reboot the device |
| `q` | Quit the file manager |

## Applications

![Solitair, one of the built-in apps, running on the device](image/solitair.jpg)

The following come preinstalled under `/home/tool` and `/home/game`.

Each app's directory also contains a README explaining how to use it!

<table>
  <tr>
    <td><img src="image/writer.png" alt="Writer" /></td>
    <td><img src="image/scheduler.png" alt="Scheduler" /></td>
    <td><img src="image/calc.png" alt="Calc" /></td>
  </tr>
  <tr>
    <td><img src="image/paint.png" alt="Paint" /></td>
    <td><img src="image/solitair.png" alt="Solitair" /></td>
    <td><img src="image/bomb.png" alt="Bomb" /></td>
  </tr>
</table>

### Writer — `/home/tool/writer`

A word processor. From business documents to poetry, write anything you like!

### Scheduler — `/home/tool/scheduler`

The greatest schedule management software.

### Calc — `/home/tool/calc`

A spreadsheet. Manage all of your money.

### Paint — `/home/tool/paint`

Draw anything!

### Solitair — `/home/game/solitair`

The world's finest card game. Compete for the high score!

### Bomb — `/home/game/bomb`

Launch it and you'll get it! That nostalgic game!

### Slide — `/home/tool/slide`

Displays numbered Markdown files as slides. Add files such as `1.md`, `2.md`,
and `3.md` to `/home/tool/slide`; they are displayed in numeric order.

Supported Markdown syntax:

- `#`, `##`, and `###` headings
- Lists starting with `-`
- Inline code enclosed in backticks
- Ruby code blocks enclosed in `` ```ruby `` and `` ``` ``

| Key | Action |
| --- | --- |
| `h` / `l` | Previous / next page |
| `j` / `k` | Scroll down / up |
| Esc | Quit |

### Gallery — `/home/tool/gallery`

A gallery of the built-in Widget components.

## Editing Code

The editor opened with `e` is a tiny vim running on the device. It has normal, insert, visual, operator, and command modes, plus search, syntax highlighting, automatic indentation, and Ruby code completion. The keys your fingers remember mostly just work!

In insert mode, press `Ctrl-N` to open completion; it also opens automatically after `.` and uppercase letters in `.rb` files.

![Editing Ruby code in the on-device vim](image/editor.jpg)

| Command | Action |
| --- | --- |
| `:w` | Save |
| `:q` | Quit (refuses if there are unsaved changes) |
| `:q!` | Quit without saving |
| `:wq` / `:x` | Save and quit |

## Compiling and Running

The device compiles `.rb` into `.mrb` (bytecode) on the spot (see `c` / `a` in the key list).
Everything runs inside a sandbox.

## Application Development

New applications should follow this layout.

### Directory Layout

An application is a single directory. Press `R` in the file manager to run it.

```
myapp/
├── main.manifest   # optional: lists the .mrb files to load, one per line
├── main.mrb        # entry point when there is no main.manifest
├── *.rb / *.mrb    # your modules
└── image.h         # optional: splash image shown at launch
```

- If `main.manifest` exists, the listed `.mrb` files are loaded into a single sandbox in order. Put dependencies first and `main.mrb` last.
- Without `main.manifest`, only `main.mrb` is executed.
- If neither exists, `No main.manifest or main.mrb` is shown.

Applications can use the built-in Widget components with `require 'area512-widget'`. See [the Widget component documentation](components/area512/mrbgems/picoruby-area512-widget/README.md) and the preinstalled `/home/tool/gallery` app.

## Building

### Requirements

- ESP-IDF v5.5+
- Ruby + Bundler
- M5 Cardputer ADV or Cardputer v1.1
- USB-C cable

### Setup

```sh
git clone --recursive git@github.com:engneer-hamachan/area512-dev.git
cd area512-dev
. $YOUR_ESP_IDF_PATH/export.sh
rake setup
```

If you already cloned without `--recursive`:

```sh
git submodule update --init --recursive
```

### Build and Flash

```sh
# Cardputer ADV build
rake build

# Cardputer v1.1 build
rake build:v1.1

rake flash
```

Files under `storage/` are embedded in the firmware as seed content and
restored to the SD card's `Area512_data/` directory on first boot (each
top-level directory is only written if it does not exist yet on the card).

## Contributing

AREA512 welcomes contributions of new apps and AREA512 artwork (splash images and such)!

## License

[MIT License](LICENSE)
