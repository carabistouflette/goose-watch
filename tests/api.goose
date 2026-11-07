// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import os;
import std;
fn emit(event: const watch::Event&) {
    var line: u8[>..] = [];
    format(line, event.mask, " ", event.cookie, " ");
    let hex = "0123456789abcdef";
    for c in event.name { line.push(hex[(c >> 4) as i64]); line.push(hex[(c & 15) as i64]); }
    print(line);
}
fn main() {
    let argv = args(); assert(argv.len == 2);
    var owner = watch::Watcher { .. };
    var event = watch::Event { .. };
    assert(watch::next(owner, 0, event).error == watch::Error.InvalidState);
    assert(watch::close(owner).error == watch::Error.None);
    for path in ["", "relative", "/a//b", "/a/.", "/a/..", "/a/", "/a\0b"] {
        assert(watch::open(owner, path).error == watch::Error.InvalidInput);
    }
    var long: u8[>..] = []; long.resize(4096, 'x'); long[0] = '/';
    assert(watch::open(owner, long).error == watch::Error.InvalidInput);
    let opened = watch::open(owner, argv[1]);
    if opened.error != watch::Error.None { print("OPEN_ERROR ", opened.os_error); return; }
    assert(watch::open(owner, argv[1]).error == watch::Error.InvalidState);
    event.mask = 99; event.cookie = 99; event.name.append("stale");
    assert(watch::next(owner, -1, event).error == watch::Error.InvalidInput);
    assert(event.mask == 0 && event.cookie == 0 && event.name.len == 0);
    assert(watch::next(owner, 60001, event).error == watch::Error.InvalidInput);
    print("READY");
    var command: u8[>..] = [];
    while read_line(command) {
        if command == "close" {
            assert(watch::close(owner).error == watch::Error.None);
            assert(watch::close(owner).error == watch::Error.None);
            assert(watch::next(owner, 0, event).error == watch::Error.InvalidState);
            print("CLOSED"); return;
        }
        if command == "reopen" {
            assert(watch::close(owner).error == watch::Error.None);
            assert(watch::open(owner, argv[1]).error == watch::Error.None);
            print("REOPENED");
        } else {
            let status = watch::next(owner, if command == "wait" { 20 } else { 0 }, event);
            if status.error == watch::Error.End { print("END"); }
            else {
                assert(status.error == watch::Error.None);
                if status.ready { emit(event); }
                else { assert(event.mask == 0 && event.cookie == 0 && event.name.len == 0); print("IDLE"); }
            }
        }
        command.clear();
    }
    assert(watch::close(owner).error == watch::Error.None);
}
