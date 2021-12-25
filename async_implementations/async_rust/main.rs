use core::ptr::null;
use std::task::Context;
use std::future::{Future};
use core::task::{RawWaker, RawWakerVTable, Waker};

unsafe fn noop_clone(_data: *const ()) -> RawWaker {
    noop_raw_waker()
}

unsafe fn noop(_data: *const ()) {}

const NOOP_WAKER_VTABLE: RawWakerVTable = RawWakerVTable::new(noop_clone, noop, noop, noop);

const fn noop_raw_waker() -> RawWaker {
    RawWaker::new(null(), &NOOP_WAKER_VTABLE)
}

#[inline]
pub fn noop_waker() -> Waker {
    unsafe { Waker::from_raw(noop_raw_waker()) }
}

async fn hello_world() {
    println!("hello, world!");
}

pub fn main() {
    let waker = noop_waker();
    let mut c: Context =  Context::from_waker(&waker);
    let mut future = Box::pin(hello_world()); 

    let future_s = future.as_mut();
    future_s.poll(&mut c);
}