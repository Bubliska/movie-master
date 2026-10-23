# Movie Master

A free, offline screenplay editor for DOS, Windows and Apple Silicon Mac.
This repository contains the application source and its English landing page.

[Website](https://www.moviemaster.app/) · [All releases](https://github.com/Bubliska/movie-master/releases)

## Download Movie Master

| Platform | Download |
| --- | --- |
| Mac · Apple Silicon | [Movie Master v0.4.4 · DMG](https://github.com/Bubliska/movie-master/releases/download/v0.4.4/Movie-Master-0.4.4-AppleSilicon.dmg) |
| DOS & Windows · 32/64-bit | [Movie Master v0.4.4 · ZIP](https://github.com/Bubliska/movie-master/releases/download/v0.4.4/Movie-Master-0.4.4-DOS-Windows.zip) |

Mac requires macOS 15 or later. This beta is not Apple-notarized; see the [Mac installation instructions](https://www.moviemaster.app/credits.html#mac-install). Extract the entire DOS/Windows ZIP before running the app.

## Writing

Six screenplay elements, selectable keyboard layouts, undo/redo, scene list,
scene move/duplicate, Fountain import, and PDF/TXT/Fountain export. PDF embeds
Courier Prime. The DOS interface is monochrome, with a steady software cursor.
The scene list opens from the menu rather than occupying the writing screen.

- F1 Slugline, F2 Action, F3 Character, F4 Parenthetical, F5 Dialog, F6 Transition.
- Tab cycles elements or completes a repeated cue. Ctrl+Enter adds an element.
- F9/Esc opens menus. F10/Ctrl+S saves. Alt+H opens help.
- First Save, Save As and every export open the in-app folder browser.
  It starts at Desktop. Enter opens folders; F5 saves here; F3 accepts a path.
- Mac Fountain import uses the native macOS file chooser, starting at Desktop.
  It supports Unicode filenames, including decomposed Turkish names hidden by
  DOSBox. A private temporary copy is read; the original is unchanged. DOS and
  Windows import use the in-app browser.
- Windows: extract the portable ZIP and run START.BAT. DOS: run MOVIEMST.EXE with
  CWSDPMI.EXE and FONT.TTF alongside it. Mac: drag Movie Master.app to Applications.

## Keyboard layouts — v0.4.4

Settings selects System, US, UK, Dvorak, Turkish Q/F, Spanish, French AZERTY,
German QWERTZ, Italian, Portuguese or Brazilian Portuguese. The selection is
saved. National layouts use PC scan codes; AltGr is Right Alt (Right Option
on Mac). Dead accents compose supported Latin letters. US/UK case uses I/i;
Turkish layouts preserve İ/i and I/ı. Existing 0/1 keyboard settings migrate.
The current 8-bit text set supports Latin text; Cyrillic, Arabic, CJK and
some symbols such as the euro sign are not supported in this release.

## Scene numbers — v0.4.3

The status bar shows the scene count as `Sc:`. `L` means numbering is locked.
Use **Scenes → Show Scene Numbers** to number headings and PDF/TXT/Fountain
output. **Scenes → Lock Scene Numbers** freezes current labels and turns their
visibility on. A scene inserted between 11 and 12 receives 11A, then 11B.
Existing labels survive edits, moving, saving/reopening, recovery and undo/redo.
Duplicated/pasted scenes receive fresh labels. Deleted labels remain reserved.
Insertions before the first scene use 0A, 0B; additions at the end use the next
unused whole number. Suffixes continue after Z as AA, AB. Inserting into an
already revised group uses its next unused suffix, without renaming old scenes.

**Unlock and Renumber** asks for confirmation and restores 1, 2, 3… in document
order. Hiding numbers does not unlock them. The list still displays labels.

New SEN documents use **MOVIEMASTER 1/2** headers. The app still opens legacy
SEN files and preserves their scene numbers. Older app versions cannot open
files written with the new header.
Fountain `#11A#` labels import as locked numbers. Fountain carries visible
labels, not the registry of deleted numbers; use SEN to preserve that registry.
Duplicate/invalid imported labels cancel import while preserving the open draft.

## Build and test

All application and website sources are at the repository root. Build commands
name their C inputs explicitly; browser_test.c is a separate test harness.
No source from the historical Movie Master or Fade In applications is included.

Native regression tests (Clang, macOS/Linux):

```sh
mkdir -p build/test build/browser
clang -std=c99 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined core.c keyboard.c scenes.c pdf.c files.c fountain.c screen.c main.c tests.c -o build/hosttest
cp FONT.TTF build/test/
(cd build/test && ../hosttest /TEST)
clang -std=c99 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined core.c keyboard.c scenes.c pdf.c files.c fountain.c tests.c browser_test.c -o build/browser-test
cp FONT.TTF build/browser/
(cd build/browser && ../browser-test)
```

Use an empty browser test directory each run. Tests create sample documents and
exports inside their working directory. FONT.TTF is required for PDF tests.
The native host build runs tests; the actual editor targets Windows or DOS.

DOS: install DJGPP 2.05/GCC 9.3 and CWSDPMI, set DJGPP and PATH, then run
`gcc @BUILD.RSP`. Use 128 MB DOSBox memory for compilation, 32 MB for the editor.
[DJGPP](https://www.delorie.com/djgpp/).

Windows (Zig 0.14.1; replace x86_64 with x86 for 32-bit):

```sh
zig cc -target x86_64-windows-gnu -std=c99 -O2 -s -Wall -Wextra -Werror core.c keyboard.c scenes.c pdf.c files.c fountain.c screen.c main.c tests.c -luser32 -lshell32 -o MOVIEMST.EXE
```

Mac: see `python3 build_mac.py --help`. Supply a portable release directory
containing DOS/, SRC/ and LICENSES/, and the official DOSBox-X SDL2 ARM64 .app.
The wrapper keeps private sessions in Application Support/MovieMaster.
On the first upgrade, it moves the previous settings/session folder without
changing its contents; a running previous app must be closed first. The DOS
executable is MOVIEMST.EXE to fit DOS 8.3 filenames.

## Website and deployment

The buildless landing page uses index.html, credits.html, style.css, app.js,
mark.svg, movie-master-dune.png, robots.txt and sitemap.xml. Serve the root
with any static HTTP server.
The image is an unretouched screenshot of the running v0.4.2 DOS app, containing
an original Arrakis/Paul/Chani demo scene, not a published screenplay excerpt.
Eric Roth sources and image credits are on the credits page; no endorsement.
Donate opens a dialog with the creator’s PayPal.Me link and USDT receiving
addresses for Ethereum (ERC20) and TRON (TRC20). Payments happen outside the site;
crypto addresses can be copied without connecting a wallet.

Import this repository in Vercel with the Other framework preset and root `.`.
vercel.json copies only the eight public site files into public/. No dependencies
are installed. App downloads link to GitHub release assets, so the website does
not reconstruct large binaries in the browser. Update both download links and
version text together when publishing a new release. A push updates the site.
Canonical URLs use https://www.moviemaster.app/. Keep the Google and Bing
verification meta tags in index.html to retain webmaster verification.

## License and limitations

Original application and website code: MIT, see LICENSE. Third-party components
retain their own licenses: see THIRD-PARTY.txt and LICENSE-* files. The Mac
release bundles unmodified DOSBox-X; GPL and source notices accompany it.

This beta was tested in DOSBox-X and with native Clang ASan/UBSan. Windows 32/64
builds compile, but no live Windows machine or physical DOS hardware was used.
Mac requires Apple Silicon and macOS 15+. The Mac app is ad-hoc signed and is not
Apple-notarized; no Intel Mac binary is included. Online collaboration, advanced
revision tracking and production scheduling are outside this app's scope.

Public visitors may view, download and fork this repository. They do not have
write access to this repository; proposed changes require the owner's review.
