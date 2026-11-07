// SPDX-License-Identifier: Apache-2.0
// Times public next + name ownership, separately from filesystem generation.
import watch from "..";
import os;
import std;
fn main() {
    let argv = args(); assert(argv.len == 3);
    let count, valid = parse_int(argv[2]); assert(valid && count >= 1 && count <= 1024);
    var owner = watch::Watcher { .. };
    assert(watch::open(owner, argv[1]).error == watch::Error.None);
    var event = watch::Event { .. };
    let idle_start = clock_ns();
    for _ in 10000 { let status = watch::next(owner, 0, event); assert(status.error == watch::Error.None && !status.ready); }
    let idle_ns = clock_ns() - idle_start;
    for i in count { assert(write_file(str(argv[1], "/sample-", i), "x")); }
    var events: i64 = 0; var creates: i64 = 0; var name_bytes: i64 = 0;
    let start = clock_ns();
    loop {
        let status = watch::next(owner, 0, event);
        assert(status.error == watch::Error.None);
        if !status.ready { break; }
        assert(!watch::has(event, watch::OVERFLOW));
        events++; name_bytes += event.name.len;
        if watch::has(event, watch::CREATE) { creates++; }
    }
    let drain_ns = clock_ns() - start;
    assert(creates == count && events >= count);
    assert(watch::close(owner).error == watch::Error.None);
    print(idle_ns, " ", drain_ns, " ", events, " ", name_bytes);
}
