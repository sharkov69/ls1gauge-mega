const fs=require('fs'),assert=require('assert');
const source=fs.readFileSync('ls1gauge-mega/display.h','utf8');
const row=source.match(/\{112,\s*225,\s*(\d+),[^\n]+RX channel/);
assert(row,'RX display field exists');assert(Number(row[1])>=5*6,'RX erase rectangle must fit 12600 -> 9999 transition');
console.log('Display: five-digit RX field erase bounds verified.');
