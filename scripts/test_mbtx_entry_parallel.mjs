import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(scriptDir, '..');
const version = '0.1.0';
const moduleName = 'i5ting/gulp';
const packageName = `mulpjs-mulp-${version}.zip`;

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: root,
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

run('moon', ['package']);

const packagePath = path.join(root, '_build', 'publish', packageName);
const packageBytes = fs.readFileSync(packagePath);
const checksum = crypto.createHash('sha256').update(packageBytes).digest('hex');
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'mulp-mbtx-entry-'));
const home = path.join(tmp, 'home');
const cacheDir = path.join(home, '.moon', 'registry', 'cache', 'mulpjs', 'mulp');
const indexDir = path.join(home, '.moon', 'registry', 'index', 'user', 'mulpjs');
const asyncCacheSrc = path.join(
  process.env.HOME,
  '.moon',
  'registry',
  'cache',
  'moonbitlang',
  'async',
);
const asyncIndexSrc = path.join(
  process.env.HOME,
  '.moon',
  'registry',
  'index',
  'user',
  'moonbitlang',
  'async.index',
);
fs.mkdirSync(cacheDir, { recursive: true });
fs.mkdirSync(indexDir, { recursive: true });
fs.mkdirSync(path.join(home, '.moon', 'registry', 'cache', 'moonbitlang'), {
  recursive: true,
});
fs.mkdirSync(path.join(home, '.moon', 'registry', 'index', 'user', 'moonbitlang'), {
  recursive: true,
});
fs.cpSync(asyncCacheSrc, path.join(home, '.moon', 'registry', 'cache', 'moonbitlang', 'async'), {
  recursive: true,
});
fs.copyFileSync(
  asyncIndexSrc,
  path.join(home, '.moon', 'registry', 'index', 'user', 'moonbitlang', 'async.index'),
);
fs.copyFileSync(packagePath, path.join(cacheDir, `${version}.zip`));
fs.writeFileSync(
  path.join(indexDir, 'mulp.index'),
  `${JSON.stringify({
    name: moduleName,
    version,
    deps: { 'moonbitlang/async': '0.19.0' },
    checksum,
    created_at: '2026-01-01T00:00:00+00:00',
  })}\n`,
);

const mbtxPath = path.join(tmp, 'gulp.mbtx');
fs.writeFileSync(
  mbtxPath,
  `///|
import {
  "moonbitlang/core/env" @env,
  "moonbitlang/async@0.19.0" @async,
  "i5ting/gulp@${version}/core" @core,
  "i5ting/gulp@${version}/entry" @entry,
}

///|
async fn main {
  let registry = @entry.new_async_registry()
  let clean = @entry.async_task(registry, "clean", async fn(
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    @async.sleep(120)
    Ok(())
  })
  let scripts = @entry.async_task(registry, "scripts", async fn(
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    @async.sleep(120)
    Ok(())
  })
  ignore(@entry.async_task_from(registry, "build", @entry.parallel([clean, scripts])))
  let ctx = @core.new_context(
    cwd=".",
    now_ms=@async.now(),
    cancellation=@core.new_cancellation_token(),
  )
  let started = @async.now()
  @entry.async_run_entry_args(registry, @env.args(), ctx, println).unwrap()
  let elapsed = @async.now() - started
  if elapsed < 220L {
    println("entry parallel ok")
  } else {
    println("entry parallel slow: \\{elapsed}")
  }
}
`,
);

const result = run('moon', ['run', '--target', 'native', mbtxPath, '--', 'build'], {
  env: { ...process.env, HOME: home },
});

if (!result.stdout.includes('entry parallel ok')) {
  throw new Error(`Expected entry parallel ok, got:\n${result.stdout}\n${result.stderr}`);
}

console.log(result.stdout.trim());
