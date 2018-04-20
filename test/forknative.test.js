const cp = require('../lib/index');
const util = require('util');

test('disconnect', done => {
  let child = cp.forkNative('./hipc');

  child.on('disconnect', () => {
    expect(child.connected).toEqual(false);
    done();
  });

  child.on('error', err => {
    expect(err).toBeFalsy();
    done();
  });

  child.on('exit', (code, signal) => {
    expect(code).toEqual(0);
  });

  expect(child.connected).toEqual(true);
  child.disconnect();
});

test('send and receive message', done => {
  let child = cp.forkNative('./hipc');

  child.on('message', (message, sendHandle) => {
    expect(message).toEqual({ cmd: 'pong' });
    child.disconnect();
    done();
  });

  child.on('error', err => {
    expect(err).toBeFalsy();
    child.disconnect();
    done();
  });

  child.on('exit', (code, signal) => {
    expect(code).toEqual(0);
  });

  expect(child.connected).toEqual(true);
  child.send({ cmd: 'ping' });
});

test('send only', done => {
  let child = cp.forkNative('./hipc');

  child.on('error', err => {
    expect(err).toBeFalsy();
    done();
  });

  child.on('exit', (code, signal) => {
    expect(code).toEqual(0);
    done();
  });

  expect(child.connected).toEqual(true);
  child.send({ cmd: 'invalid' });
  setTimeout(() => child.disconnect(), 200);
});

test('kill', done => {
  let child = cp.forkNative('./hipc');

  child.on('error', err => {
    expect(err).toBeFalsy();
    child.disconnect();
    done();
  });

  child.on('exit', (code, signal) => {
    expect(signal).toEqual('SIGTERM');
    done();
  });

  expect(child.connected).toEqual(true);
  child.kill();
});

