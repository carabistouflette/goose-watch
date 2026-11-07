// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import std;
import os;
fn main() {
    let argv = args();
    if argv.len < 2 { print("usage: sets ABS_DIRECTORY [ABS_DIRECTORY ...]"); exit(2); }
    var paths: (const u8[:])[>..] = []; for path in argv[1..] { paths.push(path); }
    var owner = watch::WatchSet { .. }; let opened = watch::open_set(owner, paths[..]);
    if opened.status.error != watch::Error.None { print("open error at index ", opened.failed_directory, ": ", opened.status.error); exit(1); }
    var out = watch::SetEvent { .. }; let status = watch::next(owner, 1000, out);
    if status.ready { print("directory=", out.directory, " mask=", out.event.mask, " cookie=", out.event.cookie); }
    else { print("no event: ", status.error); }
    let closed = watch::close(owner);
    if status.error != watch::Error.None || closed.error != watch::Error.None { exit(1); }
}
