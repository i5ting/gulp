#!/usr/bin/env node
import { createRequire } from 'node:module';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Readable, Transform, Writable, pipeline } from 'node:stream';
import { promisify } from 'node:util';

const require = createRequire(import.meta.url);
const pipe = promisify(pipeline);
const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const mulpRoot = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(mulpRoot, '..');
const gulp = require(path.join(repoRoot, 'index.js'));
const Vinyl = require('vinyl');
const vinylFs = require('vinyl-fs');

const allScenarios = [
  'pipeline-object-sink',
  'pipeline-memory-dest',
  'dest-stream-16mb',
  'dest-buffer-16mb',
  'vinyl-construct-10000',
  'vinyl-relative-10000',
  'vinyl-clone-buffer-10000',
  'vinyl-fs-src-buffer-1000',
  'vinyl-fs-dest-buffer-1000',
  'vinyl-fs-dest-stream-16mb',
];
const defaultScenarios = [
  'vinyl-construct-10000',
  'vinyl-relative-10000',
  'vinyl-clone-buffer-10000',
  'vinyl-fs-src-buffer-1000',
  'vinyl-fs-dest-buffer-1000',
  'vinyl-fs-dest-stream-16mb',
];
const warmup = 10;
const samples = 40;
const vinylFsFixture = '/tmp/mulp-vinyl-fs-bench-src';

function usage() {
  console.log(`Usage: node mulp/scripts/bench_compare.mjs [options]

Options:
  --json                 Print JSON only
  --rounds <n>           Rounds per tool/scenario (default: 5)
  --scenario <name>      Run one scenario; repeatable
  --no-build             Reuse existing release native benchmark executable
  --no-enforce           Do not fail when MoonBit native is slower than JS
  --help                 Show this help

Scenarios:
  ${allScenarios.join('\n  ')}

Default scenarios:
  ${defaultScenarios.join('\n  ')}
`);
}

function parseArgs(argv) {
  const options = {
    json: false,
    build: true,
    enforce: true,
    rounds: 5,
    scenarios: [],
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--help') {
      options.help = true;
    } else if (arg === '--json') {
      options.json = true;
    } else if (arg === '--no-build') {
      options.build = false;
    } else if (arg === '--no-enforce') {
      options.enforce = false;
    } else if (arg === '--rounds') {
      options.rounds = Number(argv[++i]);
    } else if (arg === '--scenario') {
      options.scenarios.push(argv[++i]);
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }
  if (!Number.isInteger(options.rounds) || options.rounds <= 0) {
    throw new Error('--rounds must be a positive integer');
  }
  if (options.scenarios.length === 0) {
    options.scenarios = defaultScenarios;
  }
  for (const scenario of options.scenarios) {
    if (!allScenarios.includes(scenario)) {
      throw new Error(`Unknown scenario: ${scenario}`);
    }
  }
  return options;
}

function clean(dir) {
  fs.rmSync(dir, { recursive: true, force: true });
  fs.mkdirSync(dir, { recursive: true });
}

function scenarioOutputDir(prefix, scenario) {
  if (scenario.includes('dest-buffer')) return `/tmp/${prefix}-dest-buffer`;
  if (scenario.includes('dest-stream')) return `/tmp/${prefix}-dest-stream`;
  if (scenario.startsWith('dest-')) return `/tmp/${prefix}-bench`;
  return `/tmp/${prefix}-pipeline`;
}

function prepareVinylFsFixture() {
  clean(vinylFsFixture);
  for (let index = 0; index < 1000; index += 1) {
    const subdir = path.join(vinylFsFixture, `dir-${index % 10}`);
    fs.mkdirSync(subdir, { recursive: true });
    fs.writeFileSync(
      path.join(subdir, `file-${index}.txt`),
      `content-${index}`,
    );
  }
}

function median(values) {
  const sorted = [...values].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
}

function quantile(values, q) {
  const sorted = [...values].sort((a, b) => a - b);
  if (sorted.length === 1) return sorted[0];
  const position = (sorted.length - 1) * q;
  const base = Math.floor(position);
  const rest = position - base;
  return sorted[base + 1] === undefined
    ? sorted[base]
    : sorted[base] + rest * (sorted[base + 1] - sorted[base]);
}

function stats(values) {
  const sum = values.reduce((total, value) => total + value, 0);
  const q1 = quantile(values, 0.25);
  const q3 = quantile(values, 0.75);
  return {
    n: values.length,
    median: median(values),
    mean: sum / values.length,
    min: Math.min(...values),
    max: Math.max(...values),
    q1,
    q3,
    iqr: q3 - q1,
    p95: quantile(values, 0.95),
  };
}

function fmt(value) {
  return value.toFixed(2);
}

function buildMulp() {
  const result = spawnSync('moon', ['build', 'cmd/bench_compare', '--release', '--target', 'native'], {
    cwd: mulpRoot,
    encoding: 'utf8',
    maxBuffer: 20 * 1024 * 1024,
  });
  if (result.status !== 0) {
    throw new Error(`moon build failed\n${result.stdout}\n${result.stderr}`);
  }
  fs.copyFileSync(releaseMulpExePath(), mulpExePath());
  fs.chmodSync(mulpExePath(), 0o755);
  return { stdout: result.stdout, stderr: result.stderr };
}

function releaseMulpExePath() {
  return path.join(mulpRoot, '_build/native/release/build/cmd/bench_compare/bench_compare.exe');
}

function mulpExePath() {
  return '/tmp/mulp-bench-compare.exe';
}

function runMulp(scenario) {
  if (scenario === 'vinyl-fs-src-buffer-1000') {
    prepareVinylFsFixture();
  }
  clean(scenarioOutputDir('mulp-stable', scenario));
  const result = spawnSync(mulpExePath(), [scenario], {
    encoding: 'utf8',
    maxBuffer: 20 * 1024 * 1024,
  });
  if (result.status !== 0) {
    throw new Error(
      `mulp ${scenario} failed\n${result.error?.stack || result.error || ''}\n${result.stdout || ''}\n${result.stderr || ''}`,
    );
  }
  const numeric = result.stdout
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => /^\d+(?:\.\d+)?$/.test(line))
    .map(Number);
  if (numeric.length !== samples) {
    throw new Error(`mulp ${scenario} produced ${numeric.length} samples, expected ${samples}\n${result.stdout}`);
  }
  return numeric;
}

function makeVinylFiles(count) {
  return Array.from({ length: count }, (_, index) => new Vinyl({
    cwd: '/workspace',
    base: '/workspace/src',
    path: `/workspace/src/file-${index}.txt`,
    contents: Buffer.from(`content-${index}`),
  }));
}

function makeVinylFsFiles(count) {
  return Array.from({ length: count }, (_, index) => new Vinyl({
    cwd: '/workspace',
    base: '/workspace/src',
    path: `/workspace/src/file-${index}.txt`,
    contents: Buffer.from(`content-${index}`),
  }));
}

function renameTransform() {
  return new Transform({
    objectMode: true,
    transform(file, _encoding, callback) {
      file.path = `${file.path}.copy`;
      callback(null, file);
    },
  });
}

function objectSink() {
  return new Writable({
    objectMode: true,
    write(_file, _encoding, callback) {
      callback();
    },
  });
}

function countingObjectSink() {
  let count = 0;
  const sink = new Writable({
    objectMode: true,
    write(_file, _encoding, callback) {
      count += 1;
      callback();
    },
  });
  sink.count = () => count;
  return sink;
}

function memoryDestSink(outDir) {
  const contents = new Map();
  return new Writable({
    objectMode: true,
    write(file, _encoding, callback) {
      const relative = path.relative(file.base, file.path).split(path.sep).join('/');
      contents.set(`${outDir}/${relative}`, file.contents.toString());
      callback();
    },
  });
}

async function gulpPipelineObjectSinkOnce() {
  await pipe(
    Readable.from(makeVinylFiles(1000), { objectMode: true }),
    renameTransform(),
    objectSink(),
  );
}

async function gulpPipelineMemoryDestOnce() {
  await pipe(
    Readable.from(makeVinylFiles(1000), { objectMode: true }),
    renameTransform(),
    memoryDestSink('dist'),
  );
}

function largeStream(chunks) {
  let remaining = chunks;
  const chunk = Buffer.alloc(1024 * 1024, 'x');
  return new Readable({
    read() {
      if (remaining > 0) {
        remaining -= 1;
        this.push(chunk);
      } else {
        this.push(null);
      }
    },
  });
}

async function gulpDestStream16mbOnce() {
  const file = new Vinyl({
    cwd: '/workspace',
    base: '/workspace/src',
    path: '/workspace/src/bench-large.txt',
    contents: largeStream(16),
  });
  await pipe(
    Readable.from([file], { objectMode: true }),
    gulp.dest('/tmp/gulp-stable-bench'),
    objectSink(),
  );
}

async function gulpDestBuffer16mbOnce() {
  const file = new Vinyl({
    cwd: '/workspace',
    base: '/workspace/src',
    path: '/workspace/src/bench-large.txt',
    contents: Buffer.alloc(16 * 1024 * 1024, 'x'),
  });
  await pipe(
    Readable.from([file], { objectMode: true }),
    gulp.dest('/tmp/gulp-stable-bench'),
    objectSink(),
  );
}

function vinylConstruct10000Once() {
  return makeVinylFiles(10000).length;
}

function vinylRelative10000Once() {
  let total = 0;
  for (const file of makeVinylFiles(10000)) {
    total += file.relative.length;
  }
  return total;
}

function vinylCloneBuffer10000Once() {
  const clones = makeVinylFiles(10000).map((file) => file.clone());
  return clones.length;
}

async function vinylFsSrcBuffer1000Once() {
  const sink = countingObjectSink();
  await pipe(
    vinylFs.src('**/*.txt', { cwd: vinylFsFixture }),
    sink,
  );
  return sink.count();
}

async function vinylFsDestBuffer1000Once() {
  const sink = countingObjectSink();
  await pipe(
    Readable.from(makeVinylFsFiles(1000), { objectMode: true }),
    vinylFs.dest('/tmp/js-vinyl-fs-bench-dest-buffer'),
    sink,
  );
  return sink.count();
}

async function vinylFsDestStream16mbOnce() {
  const sink = countingObjectSink();
  const file = new Vinyl({
    cwd: '/workspace',
    base: '/workspace/src',
    path: '/workspace/src/bench-large.txt',
    contents: largeStream(16),
  });
  await pipe(
    Readable.from([file], { objectMode: true }),
    vinylFs.dest('/tmp/js-vinyl-fs-bench-dest-stream'),
    sink,
  );
  return sink.count();
}

function gulpScenarioFn(scenario) {
  switch (scenario) {
    case 'pipeline-object-sink':
      return gulpPipelineObjectSinkOnce;
    case 'pipeline-memory-dest':
      return gulpPipelineMemoryDestOnce;
    case 'dest-stream-16mb':
      return gulpDestStream16mbOnce;
    case 'dest-buffer-16mb':
      return gulpDestBuffer16mbOnce;
    case 'vinyl-construct-10000':
      return vinylConstruct10000Once;
    case 'vinyl-relative-10000':
      return vinylRelative10000Once;
    case 'vinyl-clone-buffer-10000':
      return vinylCloneBuffer10000Once;
    case 'vinyl-fs-src-buffer-1000':
      return vinylFsSrcBuffer1000Once;
    case 'vinyl-fs-dest-buffer-1000':
      return vinylFsDestBuffer1000Once;
    case 'vinyl-fs-dest-stream-16mb':
      return vinylFsDestStream16mbOnce;
    default:
      throw new Error(`Unknown gulp scenario: ${scenario}`);
  }
}

async function runGulp(scenario) {
  if (scenario === 'vinyl-fs-src-buffer-1000') {
    prepareVinylFsFixture();
  }
  clean(scenarioOutputDir('gulp-stable', scenario));
  clean(scenarioOutputDir('js-vinyl-fs-bench', scenario));
  const fn = gulpScenarioFn(scenario);
  for (let i = 0; i < warmup; i += 1) {
    await fn();
  }
  const values = [];
  for (let i = 0; i < samples; i += 1) {
    const started = process.hrtime.bigint();
    const checksum = await fn();
    if (Number.isInteger(checksum) && checksum <= 0) {
      throw new Error(`${scenario} produced invalid checksum ${checksum}`);
    }
    values.push(Number(process.hrtime.bigint() - started) / 1e6);
  }
  return values;
}

async function runTool(tool, scenario, rounds, log) {
  const roundResults = [];
  for (let round = 0; round < rounds; round += 1) {
    const values = tool === 'mulp' ? runMulp(scenario) : await runGulp(scenario);
    const sampleStats = stats(values);
    roundResults.push({ round: round + 1, samples: values, stats: sampleStats });
    log(`${tool}\t${scenario}\tround=${round + 1}\tmedian=${fmt(sampleStats.median)}ms\tmean=${fmt(sampleStats.mean)}ms\tiqr=${fmt(sampleStats.iqr)}ms\tp95=${fmt(sampleStats.p95)}ms`);
  }
  const medians = roundResults.map((round) => round.stats.median);
  const allSamples = roundResults.flatMap((round) => round.samples);
  return {
    rounds: roundResults,
    medianOfMedians: median(medians),
    roundStats: stats(medians),
    allStats: stats(allSamples),
  };
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  if (options.help) {
    usage();
    return;
  }
  const log = options.json ? () => {} : console.log;
  if (options.build) {
    log('Building mulp benchmark runner...');
    buildMulp();
  }
  const result = {
    config: {
      rounds: options.rounds,
      warmup,
      samples,
      enforce: options.enforce,
      scenarios: options.scenarios,
      node: process.version,
      mulpExe: mulpExePath(),
    },
    results: {},
  };
  for (const scenario of options.scenarios) {
    result.results[scenario] = {
      mulp: await runTool('mulp', scenario, options.rounds, log),
      gulp: await runTool('gulp', scenario, options.rounds, log),
    };
  }
  for (const scenario of options.scenarios) {
    const entry = result.results[scenario];
    entry.ratio = entry.mulp.medianOfMedians / entry.gulp.medianOfMedians;
    entry.pass = entry.ratio < 1;
  }
  if (options.json) {
    console.log(JSON.stringify(result, null, 2));
  } else {
    console.log('SUMMARY');
    for (const scenario of options.scenarios) {
      const entry = result.results[scenario];
      const status = entry.pass ? 'PASS' : 'FAIL';
      console.log(`${scenario}\tmulp=${fmt(entry.mulp.medianOfMedians)}ms\tjs=${fmt(entry.gulp.medianOfMedians)}ms\tratio=${entry.ratio.toFixed(2)}x\t${status}`);
    }
  }
  const failures = Object.entries(result.results)
    .filter(([, entry]) => !entry.pass)
    .map(([scenario, entry]) => `${scenario} ratio=${entry.ratio.toFixed(2)}x`);
  if (options.enforce && failures.length > 0) {
    throw new Error(`MoonBit native was not faster than JS for: ${failures.join(', ')}`);
  }
}

main().catch((error) => {
  console.error(error.stack || error);
  process.exit(1);
});
