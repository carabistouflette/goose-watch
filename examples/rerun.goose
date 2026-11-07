// SPDX-License-Identifier: Apache-2.0
import watch from "..";
import process from "carabistouflette/goose-process@4cf1f13302ad5b42f3ca94f3c7a829c44b7ee60e";
import os;
import std;
fn execute(command: const (const u8[:])[:]) -> i64 {
    var out: u8[>..] = []; var err: u8[>..] = [];
    let options = process::Options { capture: false, stdout: process::Output.Inherit,
        stderr: process::Output.Inherit, timeout_ms: 30000 };
    let status = process::run(command, options, out, err);
    print("command: error=", status.error, " exit=", status.exit_code, " signal=", status.signal);
    if status.unreaped_pid != 0 { print("child cleanup incomplete; stopping"); return -1; }
    return if status.error == process::Error.None && status.exit_code == 0 { 1 } else { 0 };
}
fn main() {
    let argv = args();
    if argv.len < 5 || argv[3] != "--" || argv[4].len == 0 || argv[4][0] != '/' {
        print("usage: rerun ABS_DIRECTORY RUNS -- /absolute/program [args...]"); exit(2);
    }
    let runs, valid = parse_int(argv[2]);
    if !valid || runs < 1 || runs > 10000 { print("RUNS must be 1..10000"); exit(2); }
    var command: (const u8[:])[>..] = [];
    for part in argv[4..] { command.push(part); }
    var owner = watch::Watcher { .. };
    let opened = watch::open(owner, argv[1]);
    if opened.error != watch::Error.None { print("watch open failed: ", opened.error, " errno=", opened.os_error); exit(1); }
    var event = watch::Event { .. };
    var command_result = execute(command[..]);
    var completed: i64 = 1;
    var failed = command_result < 0;
    while !failed && completed < runs {
        let status = watch::next(owner, 1000, event);
        if status.error == watch::Error.Interrupted { continue; }
        if status.error != watch::Error.None { print("watch stopped: ", status.error); failed = true; break; }
        if !status.ready { continue; }
        if watch::has(event, watch::OVERFLOW) {
            // Lost events can hide watch removal. Reestablish before rebuilding.
            let closed = watch::close(owner);
            if closed.error != watch::Error.None { failed = true; break; }
            let reopened = watch::open(owner, argv[1]);
            if reopened.error != watch::Error.None { failed = true; break; }
        } else {
            if watch::has(event, watch::IGNORED | watch::DELETE_SELF | watch::MOVE_SELF | watch::UNMOUNT) {
                print("watched directory changed identity; stopping"); failed = true; break;
            }
            // Avoid reacting to compiler output or to directory changes.
            if watch::has(event, watch::IS_DIRECTORY) || !ends_with(event.name, ".goose") ||
                !watch::has(event, watch::CLOSE_WRITE | watch::MOVED_TO | watch::MOVED_FROM | watch::DELETE) { continue; }
        }
        command_result = execute(command[..]); failed = command_result < 0; completed++;
    }
    let closed = watch::close(owner);
    if failed || command_result != 1 || closed.error != watch::Error.None { exit(1); }
}
