use core::ptr::null;
use std::task::{Context, Poll};
use std::future::{Future};
use std::pin::Pin;
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

pub struct DummyFuture {
    count: u8
}

impl Future for DummyFuture {
    type Output = u8;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if self.count == 1 {
            Poll::Ready(11)
        } else {
            Pin::into_inner(self).count += 1;
            Poll::Pending
        }
    }
}


async fn hello_world() {
    let fut: DummyFuture = DummyFuture {
        count: 0
    };
    fut.await;
    println!("hello, world!");
}

pub fn main() {
    let waker = noop_waker();
    let mut c: Context =  Context::from_waker(&waker);
    let mut future = Box::pin(hello_world()); 

    if let Poll::Ready(res) = future.as_mut().poll(&mut c) {
        println!("Completed on first poll!");
    }

    if let Poll::Ready(res2) = future.as_mut().poll(&mut c) {
        println!("Completed on second poll!");
    }
}