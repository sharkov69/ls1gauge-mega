// Compare observable outputs against the last pre-refactor release.
// Run from the repository root with Visual Studio C++ build tools installed.
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const baseCommit = '87f73bf365a61d264d0a96c540848d53f1345420';
const out = '.build/compatibility';
fs.mkdirSync(out, { recursive: true });
const files = cp.execFileSync('git', ['ls-tree', '-r', '--name-only', baseCommit], { encoding: 'utf8' }).trim().split('\n');
const relevant = name => /^(ls1gauge-mega\/|libraries\/J1850-VPW-WC\/|tests\/host\/)/.test(name);
for (const file of files.filter(relevant)) {
    const target = path.join(out, 'before', file);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, cp.execFileSync('git', ['show', `${baseCommit}:${file}`], { maxBuffer: 20 * 1024 * 1024 }));
}
for (const folder of ['ls1gauge-mega', 'libraries/J1850-VPW-WC', 'tests/host']) {
    fs.cpSync(folder, path.join(out, 'after', folder), { recursive: true });
}

const jobs = [];
function injectOnce(source, anchor, replacement) {
    if (source.split(anchor).length !== 2) throw new Error(`Missing or ambiguous capture anchor: ${anchor}`);
    return source.replace(anchor, replacement);
}
for (const version of ['before', 'after']) {
    for (const name of ['csv', 'lte', 'display', 'channels']) {
        const trace = `${out}/${version}-${name}.bin`;
        if (fs.existsSync(trace)) fs.unlinkSync(trace);
    }
    const folder = `${out}/${version}/tests/host`;
    const graphics = `${out}/${version}/tests/graphics`;
    fs.cpSync('tests/graphics', graphics, { recursive: true });
    const display = fs.readFileSync(`${graphics}/display_trace.cpp`, 'utf8')
        .replace('TRACE_FILE', `"${out}/${version}-display.bin"`);
    fs.writeFileSync(`${graphics}/display_trace.cpp`, display);
    let storage = fs.readFileSync(`${folder}/storage_tests.cpp`, 'utf8');
    storage = injectOnce(storage, 'CHECK(file->date==', `{
        FILE* trace = fopen("${out}/${version}-csv.bin", "wb");
        if (!trace) return 2;
        fwrite(file->text.data(), 1, file->text.size(), trace);
        fclose(trace);
    }
    CHECK(file->date==`);
    storage = injectOnce(storage, 'auto firstLine=', `{
        FILE* trace = fopen("${out}/${version}-csv.bin", "ab");
        if (!trace) return 2;
        fwrite(excel->text.data(), 1, excel->text.size(), trace);
        fclose(trace);
    }
    auto firstLine=`);
    fs.writeFileSync(`${folder}/compat_storage.cpp`, storage);

    let modem = fs.readFileSync(`${folder}/modem_tests.cpp`, 'utf8');
    modem = injectOnce(modem, 'if(peer.bodies.size()==2){', `{
        FILE* trace = fopen("${out}/${version}-lte.bin", "wb");
        if (!trace) return 2;
        for (const auto& body : peer.bodies) fwrite(body.data(), 1, body.size(), trace);
        for (const auto& command : peer.commands) {
            fwrite(command.data(), 1, command.size(), trace);
            fputc('\\n', trace);
        }
        fclose(trace);
    }
    if(peer.bodies.size()==2){`);
    fs.writeFileSync(`${folder}/compat_modem.cpp`, modem);
    for (const name of ['storage', 'modem']) {
        jobs.push(`cl /nologo /std:c++17 /EHsc /I "${folder}" "${folder}/compat_${name}.cpp" /Fo"${out}/${version}-${name}.obj" /Fe"${out}/${version}-${name}.exe"`);
        jobs.push('if errorlevel 1 exit /b 1');
        jobs.push(`"${out}/${version}-${name}.exe"`);
        jobs.push('if errorlevel 1 exit /b 1');
    }
    jobs.push(`cl /nologo /std:c++17 /EHsc ${version === 'before' ? '/DBEFORE_REFACTOR' : ''} /I "${graphics}" /I "${folder}" "${graphics}/display_trace.cpp" /Fo"${out}/${version}-display.obj" /Fe"${out}/${version}-display.exe"`);
    jobs.push('if errorlevel 1 exit /b 1');
    jobs.push(`"${out}/${version}-display.exe"`);
    jobs.push('if errorlevel 1 exit /b 1');
    const channelTrace = fs.readFileSync('tests/host/channel_trace.cpp', 'utf8')
        .replace('TRACE_FILE', `"${out}/${version}-channels.bin"`);
    fs.writeFileSync(`${folder}/channel_trace.cpp`, channelTrace);
    jobs.push(`cl /nologo /std:c++17 /EHsc ${version === 'before' ? '/DBEFORE_REFACTOR' : ''} /I "${folder}" "${folder}/channel_trace.cpp" /Fo"${out}/${version}-channels.obj" /Fe"${out}/${version}-channels.exe"`);
    jobs.push('if errorlevel 1 exit /b 1');
    jobs.push(`"${out}/${version}-channels.exe"`);
    jobs.push('if errorlevel 1 exit /b 1');
}
fs.writeFileSync(`${out}/run.cmd`, ['@echo off', 'call "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Auxiliary/Build/vcvars64.bat" >nul', ...jobs].join('\r\n'));
const result = cp.spawnSync(process.env.ComSpec, ['/d', '/c', '.build\\compatibility\\run.cmd'], { encoding: 'utf8', windowsHide: true });
fs.writeFileSync(`${out}/build.log`, (result.stdout || '') + (result.stderr || ''));
if (result.status !== 0) {
    console.error(result.stdout, result.stderr);
    process.exit(result.status || 1);
}
for (const name of ['csv', 'lte', 'display', 'channels']) {
    const before = fs.readFileSync(`${out}/before-${name}.bin`);
    const after = fs.readFileSync(`${out}/after-${name}.bin`);
    if (!before.equals(after)) throw new Error(`${name}: output differs from ${baseCommit}`);
    console.log(`${name}: ${before.length} bytes identical to ${baseCommit.slice(0, 7)}`);
}
