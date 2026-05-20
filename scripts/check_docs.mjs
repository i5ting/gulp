#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const readme = fs.readFileSync(path.resolve(scriptDir, '..', 'README.md'), 'utf8');

const required = [
  '### Migrating from `gulpfile.js`',
  '### Migration Checklist',
  '### Current Incompatibilities',
  '### Common gulp Recipe Mappings',
  '`gulp.src(globs).pipe(plugin()).pipe(gulp.dest(outDir))`',
  '`gulp.series(clean, gulp.parallel(styles, scripts))`',
  '`gulp.watch(globs, task)`',
  'npm gulp plugins',
  'gulp.mbtx',
];

for (const text of required) {
  if (!readme.includes(text)) {
    throw new Error(`Missing README migration text: ${text}`);
  }
}

console.log('docs check ok');
