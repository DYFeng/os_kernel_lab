#![no_std]
#![no_main]

#[macro_use]
extern crate user_lib;

use user_lib::{sleep, exit, get_time, fork, waitpid};

fn sleepy() {
    let time: usize = 100;
    for i in 0..5 {
        sleep(time);
        println!("sleep {} x {} msecs.", i + 1, time);
    }
    exit(0);
}

#[no_mangle]
pub fn main() -> i32 {
    let current_time = get_time();
    let pid = fork();
    let mut xstate: i32 = 0;
    if pid == 0 {
        sleepy();
    }
    assert!(waitpid(pid as usize, &mut xstate) == pid && xstate == 0);
    println!("use {} msecs.", get_time() - current_time);
    println!("sleep pass.");
    0
}