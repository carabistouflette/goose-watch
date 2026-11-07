// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import os;
import std;
fn main() {
    let argv = args();
    if argv.len != 2 { print("usage: events ABS_DIRECTORY"); exit(2); }
    var owner = watch::Watcher { .. };
    let opened = watch::open(owner, argv[1]);
    if opened.error != watch::Error.None { print("open failed: ", opened.error); exit(1); }
    var event = watch::Event { .. };
    let status = watch::next(owner, 1000, event);
    if status.error != watch::Error.None { print("next failed: ", status.error); }
    else if !status.ready { print("no event within the poll wait"); }
    else {
        // Encode untrusted filename bytes. Never print them verbatim.
        let hex = "0123456789abcdef";
        var escaped: u8[>..] = [];
        for c in event.name { escaped.append("\\x"); escaped.push(hex[(c >> 4) as i64]); escaped.push(hex[(c & 15) as i64]); }
        print("mask=", event.mask, " cookie=", event.cookie, " name=", escaped);
        if watch::has(event, watch::OVERFLOW) { print("events lost: close/reopen and rescan"); }
    }
    let closed = watch::close(owner);
    if status.error != watch::Error.None || closed.error != watch::Error.None { exit(1); }
}
