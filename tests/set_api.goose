// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import std;
import os;
fn emit(out: const watch::SetEvent&) {
    var line: u8[>..] = []; format(line, out.directory, " ", out.event.mask, " ", out.event.cookie, " ");
    let hex = "0123456789abcdef";
    for c in out.event.name { line.push(hex[(c >> 4) as i64]); line.push(hex[(c & 15) as i64]); }
    print(line);
}
fn main() {
    let argv = args(); assert(argv.len >= 2);
    var paths: (const u8[:])[>..] = []; for path in argv[1..] { paths.push(path); }
    var owner = watch::WatchSet { .. }; var out = watch::SetEvent { .. };
    assert(watch::next(owner, 0, out).error == watch::Error.InvalidState);
    assert(watch::close(owner).error == watch::Error.None);
    var empty: (const u8[:])[0] = [];
    assert(watch::open_set(owner, empty).status.error == watch::Error.InvalidInput);
    var too_many: (const u8[:])[257] = ["/tmp"; 257];
    assert(watch::open_set(owner, too_many).status.error == watch::Error.InvalidInput);
    let opened = watch::open_set(owner, paths[..]);
    if opened.status.error != watch::Error.None {
        print("OPEN_ERROR ", opened.status.error, " ", opened.status.os_error, " ", opened.failed_directory); return;
    }
    assert(opened.failed_directory == -1);
    assert(watch::open_set(owner, empty).status.error == watch::Error.InvalidState);
    print("READY"); var command: u8[>..] = [];
    while read_line(command) {
        if command == "close" {
            var saved: u8[>..] = []; saved.append(out.event.name);
            assert(watch::close(owner).error == watch::Error.None);
            assert(out.event.name == saved);
            assert(watch::close(owner).error == watch::Error.None);
            assert(watch::next(owner, 0, out).error == watch::Error.InvalidState);
            print("CLOSED"); return;
        }
        if command == "reverse" {
            assert(watch::close(owner).error == watch::Error.None);
            paths.reverse(); assert(watch::open_set(owner, paths[..]).status.error == watch::Error.None);
            print("REOPENED");
        } else if command == "live" {
            assert(watch::open_set(owner, paths[..]).status.error == watch::Error.InvalidState);
            print("LIVE");
        } else {
            out.directory = 99; out.event.mask = 99; out.event.cookie = 99; out.event.name.append("stale");
            let status = watch::next(owner, if command == "wait" { 20 } else if command == "bad" { -1 } else { 0 }, out);
            if status.ready { assert(status.error == watch::Error.None); emit(out); }
            else {
                assert(out.directory == -1 && out.event.mask == 0 && out.event.cookie == 0 && out.event.name.len == 0);
                if command == "bad" { assert(status.error == watch::Error.InvalidInput); print("INVALID"); }
                else if status.error == watch::Error.End { print("END"); }
                else { assert(status.error == watch::Error.None); print("IDLE"); }
            }
        }
        command.clear();
    }
    assert(watch::close(owner).error == watch::Error.None);
}
