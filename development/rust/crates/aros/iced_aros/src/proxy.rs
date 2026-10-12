//! The channel through which tasks, subscriptions and the runtime deliver actions to the event
//! loop, waking the window's task when something arrives.

use crate::futures::futures::channel::mpsc;
use crate::futures::futures::{Sink, task::{Context, Poll}};
use crate::runtime::Action;
use std::pin::Pin;

use crate::ffi::Waker;

pub struct Proxy<T: 'static> {
    sender: mpsc::UnboundedSender<Action<T>>,
    waker: Waker,
}

impl<T: 'static> Clone for Proxy<T> {
    fn clone(&self) -> Self {
        Self { sender: self.sender.clone(), waker: self.waker }
    }
}

impl<T: 'static> std::fmt::Debug for Proxy<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Proxy")
    }
}

impl<T: 'static> Proxy<T> {
    pub fn new(waker: Waker) -> (Self, mpsc::UnboundedReceiver<Action<T>>) {
        let (sender, receiver) = mpsc::unbounded();
        (Self { sender, waker }, receiver)
    }

    pub fn send_action(&self, action: Action<T>) {
        let _ = self.sender.unbounded_send(action);
        self.waker.wake();
    }
}

impl<T: 'static> Sink<Action<T>> for Proxy<T> {
    type Error = mpsc::SendError;

    fn poll_ready(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        Poll::Ready(Ok(()))
    }

    fn start_send(self: Pin<&mut Self>, action: Action<T>) -> Result<(), Self::Error> {
        let result = self.sender.unbounded_send(action).map_err(|e| e.into_send_error());
        self.waker.wake();
        result
    }

    fn poll_flush(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        Poll::Ready(Ok(()))
    }

    fn poll_close(mut self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        self.sender.disconnect();
        Poll::Ready(Ok(()))
    }
}
