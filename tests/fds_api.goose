// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import os;
import std;
fn main() {
    let argv = args(); assert(argv.len == 5);
    var numbers: i64[4] = [0; 4];
    for i in 4 { let fd, valid = parse_int(argv[i + 1]); assert(valid); numbers[i] = fd; }
    var owner = watch::WatchSet { .. };
    var empty: i64[0] = [];
    assert(watch::open_set_fds(owner, empty).status.error == watch::Error.InvalidInput);
    var too_many: i64[257] = [numbers[0]; 257];
    assert(watch::open_set_fds(owner, too_many).status.error == watch::Error.InvalidInput);
    let duplicate = watch::open_set_fds(owner, [numbers[0], numbers[0]]);
    assert(duplicate.status.error == watch::Error.InvalidInput && duplicate.failed_directory == 1);
    let alias = watch::open_set_fds(owner, [numbers[0], numbers[2]]);
    assert(alias.status.error == watch::Error.SystemError && alias.failed_directory == 1);
    let regular = watch::open_set_fds(owner, [numbers[0], numbers[3]]);
    assert(regular.status.error == watch::Error.SystemError && regular.failed_directory == 1);
    let opened = watch::open_set_fds(owner, numbers[..2]);
    assert(opened.status.error == watch::Error.None && opened.failed_directory == -1);
    assert(watch::open_set_fds(owner, empty).status.error == watch::Error.InvalidState);
    let old_path = str("/proc/self/fd/", numbers[0], "/old.goose");
    let new_path = str("/proc/self/fd/", numbers[1], "/new.goose");
    assert(write_file(old_path, "old") && write_file(new_path, "new"));
    var seen: i64 = 0; var event = watch::SetEvent { .. };
    for _ in 32 {
        let status = watch::next(owner, 1000, event);
        assert(status.error == watch::Error.None && status.ready);
        if watch::has(event.event, watch::CREATE) {
            if event.event.name == "old.goose" { assert(event.directory == 0); seen |= 1; }
            if event.event.name == "new.goose" { assert(event.directory == 1); seen |= 2; }
        }
        if seen == 3 { break; }
    }
    assert(seen == 3);
    assert(watch::close(owner).error == watch::Error.None);
    // Both caller descriptors remain usable after all setup failures and close.
    assert(write_file(old_path, "still borrowed") && write_file(new_path, "still borrowed"));
    print("borrowed fd identity, origin, aliases, validation and caller ownership passed");
}
