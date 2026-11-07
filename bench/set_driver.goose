// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import std;
import os;
fn main() {
    let argv = args(); assert(argv.len == 3);
    let count, valid = parse_int(argv[2]); assert(valid && count >= 1 && count <= 256);
    var words: u8[][>..] = [];
    for i in count { words.push(str(argv[1], "/d", i)); }
    var paths: (const u8[:])[>..] = []; for word in words { paths.push(word); }
    var owner = watch::WatchSet { .. }; var out = watch::SetEvent { .. };
    let begin = clock_ns(); let opened = watch::open_set(owner, paths[..]); let open_ns = clock_ns() - begin;
    assert(opened.status.error == watch::Error.None);
    let idle_start = clock_ns();
    for _ in 1000 { let status = watch::next(owner, 0, out); assert(status.error == watch::Error.None && !status.ready); }
    let idle_ns = clock_ns() - idle_start;
    for path in paths { for i in 16 { assert(write_file(str(path, "/file", i, ".goose"), "x")); } }
    var events: i64 = 0; var creates: i64 = 0; var name_bytes: i64 = 0;
    let drain_start = clock_ns();
    loop {
        let status = watch::next(owner, 0, out); assert(status.error == watch::Error.None);
        if !status.ready { break; }
        assert(!watch::has(out.event, watch::OVERFLOW)); assert(out.directory >= 0 && out.directory < count);
        events++; name_bytes += out.event.name.len;
        if watch::has(out.event, watch::CREATE) { creates++; }
    }
    let drain_ns = clock_ns() - drain_start;
    assert(creates == count * 16 && events >= creates);
    assert(watch::close(owner).error == watch::Error.None);
    print(open_ns, " ", idle_ns, " ", drain_ns, " ", events, " ", name_bytes);
}
