"""Package the unchanged DOS editor with the official DOSBox-X ARM64 runtime."""
from pathlib import Path
import argparse
import json
import hashlib
import os
import plistlib
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, required=True, help='Official DOSBox-X SDL2 ARM64 .app')
parser.add_argument('--release', type=Path, required=True, help='Portable release with DOS/, SRC/, LICENSES/')
parser.add_argument('--output', type=Path, default=Path('build/mac'), help='New staging directory')
parser.add_argument('--dmg', type=Path, help='Optional new DMG output file')
args = parser.parse_args()
root = Path(__file__).resolve().parent
stage = args.output.resolve()
release = args.release.resolve()
runtime = args.runtime.resolve()
if stage.exists() or (args.dmg and args.dmg.exists()):
    raise SystemExit('Output already exists; choose a new path to preserve previous builds.')
for required in [runtime/'Contents/MacOS/dosbox-x', release/'DOS/MOVIEMST.EXE', root/'MovieMaster.icns']:
    if not required.is_file():
        raise SystemExit(f'Missing build input: {required}')
stage.mkdir(parents=True)
app = stage/'Movie Master.app'
shutil.copytree(runtime, app, symlinks=True)
signature = app/'Contents/_CodeSignature'
if signature.exists():
    shutil.rmtree(signature)
resources = app/'Contents/Resources'
subprocess.run(['clang', '-fobjc-arc', '-Wall', '-Wextra', '-Werror', '-O2', '-arch', 'arm64',
                '-mmacosx-version-min=15.0', '-framework', 'Cocoa', '-framework', 'UniformTypeIdentifiers', str(root/'launcher.m'),
                '-o', str(app/'Contents/MacOS/MovieMaster')], check=True)
helper = app/'Contents/Helpers/MovieMasterImport.app/Contents'
(helper/'MacOS').mkdir(parents=True)
shutil.copy2(app/'Contents/MacOS/MovieMaster', helper/'MacOS/MovieMasterImport')
with (helper/'Info.plist').open('wb') as file:
    plistlib.dump({'CFBundleIdentifier': 'app.moviemaster.import', 'CFBundleExecutable': 'MovieMasterImport',
                  'CFBundleName': 'Movie Master', 'CFBundlePackageType': 'APPL', 'LSUIElement': True}, file)
shutil.copytree(release/'DOS', resources/'DOS')
shutil.copy2(root/'MOVIEMST.MAP', resources/'MOVIEMST.MAP')
shutil.copy2(root/'MovieMaster.icns', resources/'MovieMaster.icns')
shutil.copytree(release/'LICENSES', resources/'LICENSES')
shutil.copy2(root/'LICENSE', resources/'LICENSES/MOVIE-MASTER-MIT.txt')
shutil.copy2(root/'THIRD-PARTY.txt', resources/'LICENSES/THIRD-PARTY.txt')
# COPYING and README shipped with the official runtime are beside its app bundle.
for name in ['COPYING.txt', 'README.txt']:
    source = runtime.parent/name
    if source.is_file():
        shutil.copy2(source, resources/'LICENSES'/('DOSBOX-X-'+name))
(resources/'LICENSES/DOSBOX-X-SOURCE.txt').write_text(
    'DOSBox-X 2026.10.01, commit b6abbd5, unmodified runtime. GPL v2 or later.\n'
    'Source: https://github.com/joncampbell123/dosbox-x/tree/b6abbd5\n'
    'Source archive: https://github.com/Bubliska/movie-master/releases/download/v0.4.3/DOSBox-X-Source-b6abbd5.tar.gz\n'
    'Official binary: https://github.com/joncampbell123/dosbox-x/releases/tag/dosbox-x-v2026.10.01\n')
shutil.copytree(release/'SRC', resources/'Sources/DOS')
(resources/'Sources/Mac').mkdir(parents=True)
for name in ['launcher.m', 'build_mac.py', 'README.md', 'LICENSE']:
    shutil.copy2(root/name, resources/'Sources/Mac'/name)
info = {
    'CFBundleExecutable': 'MovieMaster', 'CFBundleIdentifier': 'app.moviemaster.mac',
    'CFBundleName': 'Movie Master', 'CFBundleDisplayName': 'Movie Master',
    'CFBundlePackageType': 'APPL', 'CFBundleInfoDictionaryVersion': '6.0',
    'CFBundleShortVersionString': '0.4.5', 'CFBundleVersion': '5',
    'CFBundleIconFile': 'MovieMaster.icns', 'LSMinimumSystemVersion': '15.0',
    'LSApplicationCategoryType': 'public.app-category.productivity',
    'NSHighResolutionCapable': True, 'NSPrincipalClass': 'NSApplication',
    'NSDocumentsFolderUsageDescription': 'Movie Master opens and saves documents in folders you choose inside the app.',
    'NSDesktopFolderUsageDescription': 'Movie Master opens Desktop in its document and Fountain browser.',
    'NSHumanReadableCopyright': 'Movie Master. DOSBox-X copyright its respective authors; see LICENSES.',
}
with (app/'Contents/Info.plist').open('wb') as file:
    plistlib.dump(info, file)
shutil.copy2(app/'Contents/Info.plist', resources/'Sources/Mac/Info.plist')
(app/'Contents/PkgInfo').write_bytes(b'APPL????')
os.symlink('/Applications', stage/'Applications')
(stage/'READ ME.txt').write_text(
    'Movie Master 0.4.5 — Apple Silicon, macOS 15 or later\n\n'
    'Drag Movie Master.app to Applications. DOSBox-X is included.\n'
    'The beta is ad-hoc signed and is not Apple-notarized.\n\n'
    'F9/Esc: menus. Alt+H: help. F10: Save. The first Save asks for a folder.\n'
    'Save starts at Desktop. Type a file name, then press Enter to save.\n'
    'Tab switches between the file name and folders; arrows browse folders.\n'
    'Long file names and spaces are supported by the bundled DOSBox-X runtime.\n'
    'Import Fountain opens the macOS file chooser, including Unicode names.\n'
    'Settings selects US, UK, Dvorak and national Latin keyboard layouts.\n'
    'Scenes > Lock Scene Numbers: preserve 11, 12; insert 11A, 11B between them.\n'
    'Scenes > Unlock and Renumber restores sequential numbers after confirmation.\n'
    'Sc: is the scene count. L means numbers are locked. Use SEN to preserve locks.\n'
    'Sources and licenses are inside the app: Contents/Resources/Sources and LICENSES.\n')
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
(stage/'build-info.json').write_text(json.dumps({
    'version': '0.4.5', 'architecture': 'arm64', 'minimum_macos': '15.0',
    'dosbox_version': '2026.10.01', 'dosbox_commit': 'b6abbd5',
    'dos_sha256': hashlib.sha256((resources/'DOS/MOVIEMST.EXE').read_bytes()).hexdigest(),
    'dosbox_sha256': hashlib.sha256((app/'Contents/MacOS/dosbox-x').read_bytes()).hexdigest(),
    'signing': 'ad-hoc', 'notarized': False,
}, indent=2)+'\n')
if args.dmg:
    args.dmg.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['hdiutil', 'create', '-volname', 'Movie Master', '-srcfolder', str(stage),
                    '-format', 'UDZO', str(args.dmg.resolve())], check=True)
print(app)
