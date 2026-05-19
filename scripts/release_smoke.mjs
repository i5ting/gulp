#!/usr/bin/env node
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const mulpRoot = path.resolve(scriptDir, '..');
const releaseExe = path.join(
  mulpRoot,
  '_build',
  'native',
  'release',
  'build',
  'cmd',
  'mulp',
  'mulp.exe',
);

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: mulpRoot,
    encoding: 'utf8',
    ...options,
  });
  if (result.status !== 0) {
    throw new Error(
      [
        `Command failed: ${command} ${args.join(' ')}`,
        `status: ${result.status}`,
        result.stdout,
        result.stderr,
      ].join('\n'),
    );
  }
  return result;
}

function expect(command, args, expectedStdout, options = {}) {
  const result = run(command, args, options);
  if (result.stdout !== expectedStdout) {
    throw new Error(
      [
        `Unexpected stdout for ${command} ${args.join(' ')}`,
        `expected: ${JSON.stringify(expectedStdout)}`,
        `actual: ${JSON.stringify(result.stdout)}`,
        result.stderr,
      ].join('\n'),
    );
  }
}

run('moon', ['build', 'cmd/mulp', '--release', '--target', 'native']);

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'mulp-release-smoke-'));
const binDir = path.join(tmp, 'bin');
fs.mkdirSync(binDir, { recursive: true });
const installed = path.join(binDir, process.platform === 'win32' ? 'mulp.exe' : 'mulp');
fs.copyFileSync(releaseExe, installed);
fs.chmodSync(installed, 0o755);

const pathKey = Object.keys(process.env).find((key) => key.toLowerCase() === 'path') || 'PATH';
const env = { ...process.env };
env[pathKey] = `${binDir}${path.delimiter}${process.env[pathKey] || ''}`;

expect('mulp', ['--version'], '0.1.0\n', { env });
expect('mulp', ['--cwd', 'examples/basic', 'build'], 'example build\n', { env });
expect('mulp', ['--cwd', 'examples/basic', '--tasks'], 'build\nclean\nwatch\nparallel\n', {
  env,
});
expect('mulp', ['--cwd', 'examples/basic', '--tree', 'build'], 'root\n  build\n  clean\n  watch\n  parallel\n', {
  env,
});

console.log(`release smoke ok: ${installed}`);
