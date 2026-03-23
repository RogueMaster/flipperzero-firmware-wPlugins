#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const os = require('os');

const home = os.homedir();
const skillDir = path.join(home, '.claude', 'skills', 'claupper');
const clapperDir = path.join(home, 'claupper');
const pkgDir = path.join(__dirname, '..');

// Handle --uninstall
if (process.argv.includes('--uninstall')) {
  let removed = false;
  if (fs.existsSync(skillDir)) {
    fs.rmSync(skillDir, { recursive: true });
    console.log('  Removed skill from', skillDir);
    removed = true;
  }
  if (fs.existsSync(clapperDir)) {
    fs.rmSync(clapperDir, { recursive: true });
    console.log('  Removed files from', clapperDir);
    removed = true;
  }
  if (removed) {
    console.log('\n  Claupper uninstalled.');
  } else {
    console.log('  Nothing to uninstall.');
  }
  process.exit(0);
}

console.log('\n  Claupper Setup\n');

// 1. Install Claude Code skill
try {
  fs.mkdirSync(skillDir, { recursive: true });
  fs.copyFileSync(
    path.join(pkgDir, 'skill', 'SKILL.md'),
    path.join(skillDir, 'SKILL.md')
  );
  console.log('  [1/3] Skill installed -> ~/.claude/skills/claupper/');
} catch (err) {
  console.error('  Failed to install skill:', err.message);
  process.exit(1);
}

// 2. Copy .fap files
try {
  fs.mkdirSync(clapperDir, { recursive: true });
  const fapSrc = path.join(pkgDir, 'faps');
  for (const f of fs.readdirSync(fapSrc)) {
    fs.copyFileSync(path.join(fapSrc, f), path.join(clapperDir, f));
  }
  console.log('  [2/3] Flipper apps  -> ~/claupper/');
} catch (err) {
  console.error('  Failed to copy .fap files:', err.message);
  process.exit(1);
}

// 3. Copy macro presets
try {
  const macroDir = path.join(clapperDir, 'macros');
  fs.mkdirSync(macroDir, { recursive: true });
  const macroSrc = path.join(pkgDir, 'macros');
  for (const f of fs.readdirSync(macroSrc)) {
    fs.copyFileSync(path.join(macroSrc, f), path.join(macroDir, f));
  }
  console.log('  [3/3] Macro presets -> ~/claupper/macros/');
} catch (err) {
  console.error('  Failed to copy macros:', err.message);
  process.exit(1);
}

console.log('\n  Done! Next steps:\n');
console.log('  Claude Code:');
console.log('    Type /claupper in any Claude Code session to activate remote mode.\n');
console.log('  Flipper Zero:');
console.log('    Copy the .fap file to your Flipper SD card:');
console.log('    - BLE (Momentum/Unleashed): ~/claupper/claude_remote_ble.fap');
console.log('      -> SD/apps/Bluetooth/');
console.log('    - USB (stock firmware):     ~/claupper/claude_remote_usb.fap');
console.log('      -> SD/apps/Tools/\n');
console.log('  Macros (optional):');
console.log('    Copy a macro preset to your Flipper SD card:');
console.log('    ~/claupper/macros/default.txt -> SD/apps_data/claude_remote_ble/macros.txt\n');
console.log('  Uninstall:');
console.log('    npx claupper --uninstall\n');
